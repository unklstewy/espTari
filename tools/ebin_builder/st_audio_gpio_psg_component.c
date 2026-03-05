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
    uint8_t psg_regs[16];
    uint8_t reg_select;
    uint8_t gpio_port_a;
    uint8_t gpio_port_b;
    uint32_t ticks;
    bool initialized;
    bool irq;
} s_psg;

static int psg_init(io_config_t *config)
{
    (void)config;
    for (int i = 0; i < 16; i++) {
        s_psg.psg_regs[i] = 0;
    }
    s_psg.reg_select = 0;
    s_psg.gpio_port_a = 0;
    s_psg.gpio_port_b = 0;
    s_psg.ticks = 0;
    s_psg.irq = false;
    s_psg.initialized = true;
    return 0;
}

static void psg_reset(void)
{
    for (int i = 0; i < 16; i++) {
        s_psg.psg_regs[i] = 0;
    }
    s_psg.reg_select = 0;
    s_psg.gpio_port_a = 0;
    s_psg.gpio_port_b = 0;
    s_psg.ticks = 0;
    s_psg.irq = false;
}

static void psg_shutdown(void)
{
    s_psg.initialized = false;
}

static uint8_t psg_read_byte(uint32_t addr)
{
    switch (addr & 0x03u) {
        case 0:
            return s_psg.reg_select;
        case 1:
            return s_psg.psg_regs[s_psg.reg_select & 0x0Fu];
        case 2:
            return s_psg.gpio_port_a;
        default:
            return s_psg.gpio_port_b;
    }
}

static uint16_t psg_read_word(uint32_t addr)
{
    uint8_t hi = psg_read_byte(addr);
    uint8_t lo = psg_read_byte(addr + 1u);
    return (uint16_t)((hi << 8) | lo);
}

static void psg_write_byte(uint32_t addr, uint8_t val)
{
    switch (addr & 0x03u) {
        case 0:
            s_psg.reg_select = (uint8_t)(val & 0x0Fu);
            break;
        case 1:
            s_psg.psg_regs[s_psg.reg_select & 0x0Fu] = val;
            break;
        case 2:
            s_psg.gpio_port_a = val;
            break;
        default:
            s_psg.gpio_port_b = val;
            break;
    }
    if ((val & 0x80u) != 0u) {
        s_psg.irq = true;
    }
}

static void psg_write_word(uint32_t addr, uint16_t val)
{
    psg_write_byte(addr, (uint8_t)((val >> 8) & 0xFFu));
    psg_write_byte(addr + 1u, (uint8_t)(val & 0xFFu));
}

static void psg_clock(int cycles)
{
    if (cycles > 0) {
        s_psg.ticks += (uint32_t)cycles;
        if ((s_psg.ticks % 2048u) == 0u) {
            s_psg.irq = true;
        }
    }
}

static bool psg_irq_pending(void)
{
    return s_psg.irq;
}

static uint8_t psg_get_vector(void)
{
    return 0x50;
}

static void psg_irq_ack(void)
{
    s_psg.irq = false;
}

static void psg_set_bus(bus_interface_t *bus)
{
    (void)bus;
}

static bool psg_bus_held(void)
{
    return false;
}

static const io_interface_t s_interface = {
    .interface_version = 0x00010000,
    .name = "st.audio_gpio.psg.stub",
    .init = psg_init,
    .reset = psg_reset,
    .shutdown = psg_shutdown,
    .read_byte = psg_read_byte,
    .read_word = psg_read_word,
    .write_byte = psg_write_byte,
    .write_word = psg_write_word,
    .clock = psg_clock,
    .irq_pending = psg_irq_pending,
    .get_vector = psg_get_vector,
    .irq_ack = psg_irq_ack,
    .set_bus = psg_set_bus,
    .bus_held = psg_bus_held,
};

void* __attribute__((section(".text.component_entry"))) component_entry(void)
{
    return (void*)&s_interface;
}