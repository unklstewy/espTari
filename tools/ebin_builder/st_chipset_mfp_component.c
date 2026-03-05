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
    uint16_t timer_reload[4];
    uint16_t timer_count[4];
    uint8_t timer_ctrl[4];
    uint8_t pending_mask;
    uint8_t in_service_mask;
    uint8_t enable_mask;
    uint8_t mask_mask;
    bool initialized;
    bool irq;
} s_mfp;

enum {
    MFP_REG_IERA = 0x07,
    MFP_REG_IERB = 0x09,
    MFP_REG_IPRA = 0x0B,
    MFP_REG_IPRB = 0x0D,
    MFP_REG_ISRA = 0x0F,
    MFP_REG_ISRB = 0x11,
    MFP_REG_IMRA = 0x13,
    MFP_REG_IMRB = 0x15,
    MFP_REG_VR = 0x17,
    MFP_REG_TACR = 0x19,
    MFP_REG_TBCR = 0x1B,
    MFP_REG_TCDCR = 0x1D,
    MFP_REG_TADR = 0x1F,
    MFP_REG_TBDR = 0x21,
    MFP_REG_TCDR = 0x23,
    MFP_REG_TDDR = 0x25,
};

static void recompute_irq(void)
{
    uint8_t active = (uint8_t)(s_mfp.pending_mask & s_mfp.enable_mask & s_mfp.mask_mask);
    s_mfp.irq = (active != 0u);
    s_mfp.regs[MFP_REG_IPRB] = active;
    s_mfp.regs[MFP_REG_ISRB] = s_mfp.in_service_mask;
}

static void timer_tick(int index)
{
    if (s_mfp.timer_ctrl[index] == 0u) {
        return;
    }
    if (s_mfp.timer_count[index] > 0u) {
        s_mfp.timer_count[index]--;
    }
    if (s_mfp.timer_count[index] == 0u) {
        s_mfp.pending_mask |= (uint8_t)(1u << index);
        s_mfp.timer_count[index] = s_mfp.timer_reload[index] == 0u ? 256u : s_mfp.timer_reload[index];
    }
}

static int mfp_init(io_config_t *config)
{
    (void)config;
    for (int i = 0; i < 256; i++) {
        s_mfp.regs[i] = 0;
    }
    s_mfp.ticks = 0;
    for (int i = 0; i < 4; i++) {
        s_mfp.timer_reload[i] = 256u;
        s_mfp.timer_count[i] = 256u;
        s_mfp.timer_ctrl[i] = 0;
    }
    s_mfp.pending_mask = 0;
    s_mfp.in_service_mask = 0;
    s_mfp.enable_mask = 0;
    s_mfp.mask_mask = 0xFFu;
    s_mfp.regs[MFP_REG_VR] = 0x40u;
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
    for (int i = 0; i < 4; i++) {
        s_mfp.timer_reload[i] = 256u;
        s_mfp.timer_count[i] = 256u;
        s_mfp.timer_ctrl[i] = 0;
    }
    s_mfp.pending_mask = 0;
    s_mfp.in_service_mask = 0;
    s_mfp.enable_mask = 0;
    s_mfp.mask_mask = 0xFFu;
    s_mfp.regs[MFP_REG_VR] = 0x40u;
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
    uint8_t reg = (uint8_t)(addr & 0xFFu);
    s_mfp.regs[reg] = val;

    switch (reg) {
        case MFP_REG_IERB:
            s_mfp.enable_mask = val;
            break;
        case MFP_REG_IMRB:
            s_mfp.mask_mask = val;
            break;
        case MFP_REG_IPRB:
            s_mfp.pending_mask &= (uint8_t)~val;
            break;
        case MFP_REG_ISRB:
            s_mfp.in_service_mask &= (uint8_t)~val;
            break;
        case MFP_REG_TACR:
            s_mfp.timer_ctrl[0] = (uint8_t)(val & 0x07u);
            break;
        case MFP_REG_TBCR:
            s_mfp.timer_ctrl[1] = (uint8_t)(val & 0x07u);
            break;
        case MFP_REG_TCDCR:
            s_mfp.timer_ctrl[2] = (uint8_t)((val >> 4) & 0x07u);
            s_mfp.timer_ctrl[3] = (uint8_t)(val & 0x07u);
            break;
        case MFP_REG_TADR:
            s_mfp.timer_reload[0] = (val == 0u) ? 256u : val;
            s_mfp.timer_count[0] = s_mfp.timer_reload[0];
            break;
        case MFP_REG_TBDR:
            s_mfp.timer_reload[1] = (val == 0u) ? 256u : val;
            s_mfp.timer_count[1] = s_mfp.timer_reload[1];
            break;
        case MFP_REG_TCDR:
            s_mfp.timer_reload[2] = (val == 0u) ? 256u : val;
            s_mfp.timer_count[2] = s_mfp.timer_reload[2];
            break;
        case MFP_REG_TDDR:
            s_mfp.timer_reload[3] = (val == 0u) ? 256u : val;
            s_mfp.timer_count[3] = s_mfp.timer_reload[3];
            break;
        default:
            break;
    }

    recompute_irq();
}

static void mfp_write_word(uint32_t addr, uint16_t val)
{
    s_mfp.regs[addr & 0xFFu] = (uint8_t)((val >> 8) & 0xFFu);
    s_mfp.regs[(addr + 1u) & 0xFFu] = (uint8_t)(val & 0xFFu);
}

static void mfp_clock(int cycles)
{
    if (cycles > 0) {
        uint32_t budget = (uint32_t)cycles;

        while (budget-- > 0u) {
            s_mfp.ticks++;
            timer_tick(0);
            if ((s_mfp.ticks & 0x01u) == 0u) {
                timer_tick(1);
            }
            if ((s_mfp.ticks & 0x03u) == 0u) {
                timer_tick(2);
            }
            if ((s_mfp.ticks & 0x07u) == 0u) {
                timer_tick(3);
            }
        }

        recompute_irq();
    }
}

static bool mfp_irq_pending(void)
{
    return s_mfp.irq;
}

static uint8_t mfp_get_vector(void)
{
    uint8_t base = (uint8_t)(s_mfp.regs[MFP_REG_VR] & 0xF0u);
    for (uint8_t bit = 0; bit < 8u; bit++) {
        uint8_t mask = (uint8_t)(1u << bit);
        if ((s_mfp.pending_mask & s_mfp.enable_mask & s_mfp.mask_mask & mask) != 0u) {
            s_mfp.in_service_mask |= mask;
            return (uint8_t)(base + bit);
        }
    }
    return base;
}

static void mfp_irq_ack(void)
{
    s_mfp.pending_mask = 0;
    s_mfp.in_service_mask = 0;
    recompute_irq();
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
    .name = "st.chipset.mfp",
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