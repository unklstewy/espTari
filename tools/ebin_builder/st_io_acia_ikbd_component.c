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
    uint8_t ikbd_queue[16];
    uint8_t ikbd_head;
    uint8_t ikbd_tail;
    uint32_t ticks;
    bool initialized;
    bool irq;
} s_acia;

static int acia_init(io_config_t *config)
{
    (void)config;
    s_acia.acia_status = 0x02;
    s_acia.acia_control = 0x15;
    s_acia.acia_tx = 0;
    s_acia.acia_rx = 0;
    for (int i = 0; i < 16; i++) {
        s_acia.ikbd_queue[i] = 0;
    }
    s_acia.ikbd_head = 0;
    s_acia.ikbd_tail = 0;
    s_acia.ticks = 0;
    s_acia.irq = false;
    s_acia.initialized = true;
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
    s_acia.ticks = 0;
    s_acia.irq = false;
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
        default: return s_acia.acia_rx;
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
            break;
        case 2:
            s_acia.acia_tx = val;
            break;
        default:
            s_acia.acia_rx = val;
            break;
    }
    if ((val & 0x80u) != 0) {
        s_acia.irq = true;
    }
}

static void acia_write_word(uint32_t addr, uint16_t val)
{
    acia_write_byte(addr, (uint8_t)((val >> 8) & 0xFFu));
    acia_write_byte(addr + 1u, (uint8_t)(val & 0xFFu));
}

static void acia_clock(int cycles)
{
    if (cycles > 0) {
        s_acia.ticks += (uint32_t)cycles;
        if ((s_acia.ticks % 512u) == 0u) {
            s_acia.irq = true;
            s_acia.acia_status |= 0x80u;
        }
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
    s_acia.acia_status &= (uint8_t)~0x80u;
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
    .name = "st.io.acia_ikbd.stub",
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