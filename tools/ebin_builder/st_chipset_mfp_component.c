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
    uint8_t regs[256];
    uint32_t ticks;
    bool initialized;
    bool irq;
} s_mfp;

static int mfp_init(io_config_t *config)
{
    (void)config;
    for (int i = 0; i < 256; i++) {
        s_mfp.regs[i] = 0;
    }
    s_mfp.ticks = 0;
    s_mfp.irq = false;
    s_mfp.initialized = true;
    return 0;
}

static void mfp_reset(void)
{
    for (int i = 0; i < 256; i++) {
        s_mfp.regs[i] = 0;
    }
    s_mfp.ticks = 0;
    s_mfp.irq = false;
}

static void mfp_shutdown(void)
{
    s_mfp.initialized = false;
}

static uint8_t mfp_read_byte(uint32_t addr)
{
    return s_mfp.regs[addr & 0xFFu];
}

static uint16_t mfp_read_word(uint32_t addr)
{
    uint8_t hi = s_mfp.regs[addr & 0xFFu];
    uint8_t lo = s_mfp.regs[(addr + 1u) & 0xFFu];
    return (uint16_t)((hi << 8) | lo);
}

static void mfp_write_byte(uint32_t addr, uint8_t val)
{
    s_mfp.regs[addr & 0xFFu] = val;
    if ((addr & 0xFFu) == 0x11u) {
        s_mfp.irq = (val & 0x01u) != 0;
    }
}

static void mfp_write_word(uint32_t addr, uint16_t val)
{
    s_mfp.regs[addr & 0xFFu] = (uint8_t)((val >> 8) & 0xFFu);
    s_mfp.regs[(addr + 1u) & 0xFFu] = (uint8_t)(val & 0xFFu);
}

static void mfp_clock(int cycles)
{
    if (cycles > 0) {
        s_mfp.ticks += (uint32_t)cycles;
        if ((s_mfp.ticks & 0x3FFu) == 0) {
            s_mfp.irq = true;
        }
    }
}

static bool mfp_irq_pending(void)
{
    return s_mfp.irq;
}

static uint8_t mfp_get_vector(void)
{
    return 0x48;
}

static void mfp_irq_ack(void)
{
    s_mfp.irq = false;
}

static void mfp_set_bus(bus_interface_t *bus)
{
    (void)bus;
}

static bool mfp_bus_held(void)
{
    return false;
}

static const io_interface_t s_interface = {
    .interface_version = 0x00010000,
    .name = "st.chipset.mfp.stub",
    .init = mfp_init,
    .reset = mfp_reset,
    .shutdown = mfp_shutdown,
    .read_byte = mfp_read_byte,
    .read_word = mfp_read_word,
    .write_byte = mfp_write_byte,
    .write_word = mfp_write_word,
    .clock = mfp_clock,
    .irq_pending = mfp_irq_pending,
    .get_vector = mfp_get_vector,
    .irq_ack = mfp_irq_ack,
    .set_bus = mfp_set_bus,
    .bus_held = mfp_bus_held,
};

void* __attribute__((section(".text.component_entry"))) component_entry(void)
{
    return (void*)&s_interface;
}