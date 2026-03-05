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
    uint8_t regs[512];
    uint32_t ticks;
    uint16_t hslots;
    uint16_t vlines;
    uint8_t arbitration_owner;
    uint16_t dma_hold_cycles;
    bool irq;
    bool initialized;
} s_chipset;

static uint16_t reg_index(uint32_t addr)
{
    return (uint16_t)(addr & 0x01FFu);
}

static void update_arbitration_state(void)
{
    uint8_t rotate = (uint8_t)(s_chipset.ticks & 0x03u);
    s_chipset.arbitration_owner = rotate;
    s_chipset.regs[0x12u] = rotate;
    if (s_chipset.dma_hold_cycles > 0) {
        s_chipset.dma_hold_cycles--;
    }
}

static int chipset_init(io_config_t *config)
{
    (void)config;
    for (int i = 0; i < 512; i++) {
        s_chipset.regs[i] = 0;
    }
    s_chipset.ticks = 0;
    s_chipset.hslots = 0;
    s_chipset.vlines = 0;
    s_chipset.arbitration_owner = 0;
    s_chipset.dma_hold_cycles = 0;
    s_chipset.irq = false;
    s_chipset.regs[0x00u] = 0x01u;
    s_chipset.regs[0x80u] = 0x02u;
    s_chipset.regs[0x100u] = 0x04u;
    s_chipset.initialized = true;
    return 0;
}

static void chipset_reset(void)
{
    for (int i = 0; i < 512; i++) {
        s_chipset.regs[i] = 0;
    }
    s_chipset.ticks = 0;
    s_chipset.hslots = 0;
    s_chipset.vlines = 0;
    s_chipset.arbitration_owner = 0;
    s_chipset.dma_hold_cycles = 0;
    s_chipset.irq = false;
    s_chipset.regs[0x00u] = 0x01u;
    s_chipset.regs[0x80u] = 0x02u;
    s_chipset.regs[0x100u] = 0x04u;
}

static void chipset_shutdown(void)
{
    s_chipset.initialized = false;
}

static uint8_t chipset_read_byte(uint32_t addr)
{
    uint16_t index = reg_index(addr);
    if (index == 0x10u) {
        return (uint8_t)(s_chipset.hslots & 0xFFu);
    }
    if (index == 0x11u) {
        return (uint8_t)(s_chipset.vlines & 0xFFu);
    }
    if (index == 0x12u) {
        return s_chipset.arbitration_owner;
    }
    return s_chipset.regs[index];
}

static uint16_t chipset_read_word(uint32_t addr)
{
    uint8_t hi = s_chipset.regs[addr & 0x1FFu];
    uint8_t lo = s_chipset.regs[(addr + 1u) & 0x1FFu];
    return (uint16_t)((hi << 8) | lo);
}

static void chipset_write_byte(uint32_t addr, uint8_t val)
{
    uint16_t index = reg_index(addr);
    s_chipset.regs[index] = val;

    if (index == 0x13u) {
        s_chipset.dma_hold_cycles = (uint16_t)(16u + (uint16_t)(val & 0x1Fu) * 4u);
    }
    if (index == 0x101u && (val & 0x80u) != 0u) {
        s_chipset.irq = true;
    }
}

static void chipset_write_word(uint32_t addr, uint16_t val)
{
    s_chipset.regs[addr & 0x1FFu] = (uint8_t)((val >> 8) & 0xFFu);
    s_chipset.regs[(addr + 1u) & 0x1FFu] = (uint8_t)(val & 0xFFu);
}

static void chipset_clock(int cycles)
{
    if (cycles > 0) {
        uint32_t budget = (uint32_t)cycles;
        s_chipset.ticks += budget;
        while (budget-- > 0u) {
            s_chipset.hslots++;
            if (s_chipset.hslots >= 512u) {
                s_chipset.hslots = 0;
                s_chipset.vlines++;
                if ((s_chipset.vlines % 313u) == 0u) {
                    s_chipset.irq = true;
                    s_chipset.regs[0x14u] |= 0x01u;
                }
            }
            update_arbitration_state();
        }
    }
}

static bool chipset_irq_pending(void)
{
    return s_chipset.irq;
}

static uint8_t chipset_get_vector(void)
{
    return 0x68;
}

static void chipset_irq_ack(void)
{
    s_chipset.irq = false;
    s_chipset.regs[0x14u] &= (uint8_t)~0x01u;
}

static void chipset_set_bus(bus_interface_t *bus)
{
    (void)bus;
}

static bool chipset_bus_held(void)
{
    return s_chipset.dma_hold_cycles != 0u;
}

static const io_interface_t s_interface = {
    .interface_version = 0x00010000,
    .name = "st.chipset.glue_mmu_shifter",
    .init = chipset_init,
    .reset = chipset_reset,
    .shutdown = chipset_shutdown,
    .read_byte = chipset_read_byte,
    .read_word = chipset_read_word,
    .write_byte = chipset_write_byte,
    .write_word = chipset_write_word,
    .clock = chipset_clock,
    .irq_pending = chipset_irq_pending,
    .get_vector = chipset_get_vector,
    .irq_ack = chipset_irq_ack,
    .set_bus = chipset_set_bus,
    .bus_held = chipset_bus_held,
};

void* __attribute__((section(".text.component_entry"))) component_entry(void)
{
    return (void*)&s_interface;
}