#include <stdint.h>
#include <stdbool.h>

typedef struct bus_interface_s bus_interface_t;

typedef struct {
    void *context;
} io_config_t;

typedef struct {
    uint32_t interface_version;
    const char *name;
    int  (*init)(io_config_t *config);
    void (*reset)(void);
    void (*shutdown)(void);
    uint8_t  (*read_byte)(uint32_t addr);
    uint16_t (*read_word)(uint32_t addr);
    void     (*write_byte)(uint32_t addr, uint8_t val);
    void     (*write_word)(uint32_t addr, uint16_t val);
    void (*clock)(int cycles);
    bool (*irq_pending)(void);
    uint8_t (*get_vector)(void);
    void (*irq_ack)(void);
    void (*set_bus)(bus_interface_t *bus);
    bool (*bus_held)(void);
} io_interface_t;

static struct {
    uint8_t acia_status;
    uint8_t acia_control;
    uint8_t acia_tx;
    uint8_t acia_rx;
    uint8_t ikbd_queue[64];
    uint8_t ikbd_head;
    uint8_t ikbd_tail;
    uint8_t tx_queue[32];
    uint8_t tx_head;
    uint8_t tx_tail;
    uint16_t baud_divider;
    uint16_t baud_counter;
    uint32_t ticks;
    bool initialized;
    bool irq;
} s_acia;

enum {
    ACIA_STATUS_RX_FULL = 0x01,
    ACIA_STATUS_TX_EMPTY = 0x02,
    ACIA_STATUS_IRQ = 0x80,
};

static bool ring_empty(uint8_t head, uint8_t tail)
{
    return head == tail;
}

static uint8_t ring_next(uint8_t value, uint8_t mask)
{
    return (uint8_t)((value + 1u) & mask);
}

static bool ikbd_push(uint8_t value)
{
    uint8_t next = ring_next(s_acia.ikbd_tail, 0x3Fu);
    if (next == s_acia.ikbd_head) {
        return false;
    }
    s_acia.ikbd_queue[s_acia.ikbd_tail] = value;
    s_acia.ikbd_tail = next;
    return true;
}

static bool ikbd_pop(uint8_t *out)
{
    if (ring_empty(s_acia.ikbd_head, s_acia.ikbd_tail)) {
        return false;
    }
    *out = s_acia.ikbd_queue[s_acia.ikbd_head];
    s_acia.ikbd_head = ring_next(s_acia.ikbd_head, 0x3Fu);
    return true;
}

static bool tx_push(uint8_t value)
{
    uint8_t next = ring_next(s_acia.tx_tail, 0x1Fu);
    if (next == s_acia.tx_head) {
        return false;
    }
    s_acia.tx_queue[s_acia.tx_tail] = value;
    s_acia.tx_tail = next;
    return true;
}

static bool tx_pop(uint8_t *out)
{
    if (ring_empty(s_acia.tx_head, s_acia.tx_tail)) {
        return false;
    }
    *out = s_acia.tx_queue[s_acia.tx_head];
    s_acia.tx_head = ring_next(s_acia.tx_head, 0x1Fu);
    return true;
}

static void recompute_status(void)
{
    if (ring_empty(s_acia.ikbd_head, s_acia.ikbd_tail)) {
        s_acia.acia_status &= (uint8_t)~ACIA_STATUS_RX_FULL;
    } else {
        s_acia.acia_status |= ACIA_STATUS_RX_FULL;
    }

    if (ring_empty(s_acia.tx_head, s_acia.tx_tail)) {
        s_acia.acia_status |= ACIA_STATUS_TX_EMPTY;
    } else {
        s_acia.acia_status &= (uint8_t)~ACIA_STATUS_TX_EMPTY;
    }

    if (s_acia.irq) {
        s_acia.acia_status |= ACIA_STATUS_IRQ;
    } else {
        s_acia.acia_status &= (uint8_t)~ACIA_STATUS_IRQ;
    }
}

static void emit_ikbd_periodic_frame(void)
{
    uint8_t packet[3];
    packet[0] = 0xF8u;
    packet[1] = (uint8_t)(s_acia.ticks & 0x3Fu);
    packet[2] = (uint8_t)((s_acia.ticks >> 6) & 0x3Fu);

    bool pushed = true;
    for (int i = 0; i < 3; i++) {
        pushed = pushed && ikbd_push(packet[i]);
    }
    if (pushed) {
        s_acia.irq = true;
    }
}

static int acia_init(io_config_t *config)
{
    (void)config;
    s_acia.acia_status = 0x02;
    s_acia.acia_control = 0x15;
    s_acia.acia_tx = 0;
    s_acia.acia_rx = 0;
    for (int i = 0; i < 64; i++) {
        s_acia.ikbd_queue[i] = 0;
    }
    for (int i = 0; i < 32; i++) {
        s_acia.tx_queue[i] = 0;
    }
    s_acia.ikbd_head = 0;
    s_acia.ikbd_tail = 0;
    s_acia.tx_head = 0;
    s_acia.tx_tail = 0;
    s_acia.baud_divider = 256u;
    s_acia.baud_counter = s_acia.baud_divider;
    s_acia.ticks = 0;
    s_acia.irq = false;
    s_acia.initialized = true;
    recompute_status();
    return 0;
}

static void acia_reset(void)
{
    s_acia.acia_status = 0x02;
    s_acia.acia_control = 0x15;
    s_acia.acia_tx = 0;
    s_acia.acia_rx = 0;
    s_acia.ikbd_head = 0;
    s_acia.ikbd_tail = 0;
    s_acia.tx_head = 0;
    s_acia.tx_tail = 0;
    s_acia.baud_divider = 256u;
    s_acia.baud_counter = s_acia.baud_divider;
    s_acia.ticks = 0;
    s_acia.irq = false;
    recompute_status();
}

static void acia_shutdown(void)
{
    s_acia.initialized = false;
}

static uint8_t acia_read_byte(uint32_t addr)
{
    switch (addr & 0x03u) {
        case 0: return s_acia.acia_status;
        case 1: return s_acia.acia_control;
        case 2: return s_acia.acia_tx;
        default: {
            uint8_t value = s_acia.acia_rx;
            uint8_t next_rx = 0;
            if (ikbd_pop(&next_rx)) {
                s_acia.acia_rx = next_rx;
            }
            recompute_status();
            return value;
        }
    }
}

static uint16_t acia_read_word(uint32_t addr)
{
    uint8_t hi = acia_read_byte(addr);
    uint8_t lo = acia_read_byte(addr + 1u);
    return (uint16_t)((hi << 8) | lo);
}

static void acia_write_byte(uint32_t addr, uint8_t val)
{
    switch (addr & 0x03u) {
        case 0:
            s_acia.acia_status = val;
            break;
        case 1:
            s_acia.acia_control = val;
            s_acia.baud_divider = (uint16_t)(32u << (val & 0x03u));
            if (s_acia.baud_divider == 0u) {
                s_acia.baud_divider = 256u;
            }
            s_acia.baud_counter = s_acia.baud_divider;
            break;
        case 2:
            s_acia.acia_tx = val;
            tx_push(val);
            break;
        default:
            s_acia.acia_rx = val;
            break;
    }
    if ((val & 0x80u) != 0) {
        s_acia.irq = true;
    }
    recompute_status();
}

static void acia_write_word(uint32_t addr, uint16_t val)
{
    acia_write_byte(addr, (uint8_t)((val >> 8) & 0xFFu));
    acia_write_byte(addr + 1u, (uint8_t)(val & 0xFFu));
}

static void acia_clock(int cycles)
{
    if (cycles > 0) {
        uint32_t budget = (uint32_t)cycles;
        while (budget-- > 0u) {
            s_acia.ticks++;

            if (s_acia.baud_counter > 0u) {
                s_acia.baud_counter--;
            }
            if (s_acia.baud_counter == 0u) {
                uint8_t tx;
                if (tx_pop(&tx)) {
                    ikbd_push(tx);
                    s_acia.acia_rx = tx;
                    s_acia.irq = true;
                }
                s_acia.baud_counter = s_acia.baud_divider;
            }

            if ((s_acia.ticks % 1024u) == 0u) {
                emit_ikbd_periodic_frame();
            }
        }
        recompute_status();
    }
}

static bool acia_irq_pending(void)
{
    return s_acia.irq;
}

static uint8_t acia_get_vector(void)
{
    return 0x40;
}

static void acia_irq_ack(void)
{
    s_acia.irq = false;
    recompute_status();
}

static void acia_set_bus(bus_interface_t *bus)
{
    (void)bus;
}

static bool acia_bus_held(void)
{
    return false;
}

static const io_interface_t s_interface = {
    .interface_version = 0x00010000,
    .name = "st.io.acia_ikbd",
    .init = acia_init,
    .reset = acia_reset,
    .shutdown = acia_shutdown,
    .read_byte = acia_read_byte,
    .read_word = acia_read_word,
    .write_byte = acia_write_byte,
    .write_word = acia_write_word,
    .clock = acia_clock,
    .irq_pending = acia_irq_pending,
    .get_vector = acia_get_vector,
    .irq_ack = acia_irq_ack,
    .set_bus = acia_set_bus,
    .bus_held = acia_bus_held,
};

void* __attribute__((section(".text.component_entry"))) component_entry(void)
{
    return (void*)&s_interface;
}