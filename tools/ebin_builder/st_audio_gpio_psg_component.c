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
    uint16_t tone_counter[3];
    uint8_t tone_level[3];
    uint16_t envelope_counter;
    uint8_t envelope_level;
    uint32_t ticks;
    bool initialized;
    bool irq;
} s_psg;

static uint16_t tone_period(int channel)
{
    uint8_t lo = s_psg.psg_regs[channel * 2];
    uint8_t hi = (uint8_t)(s_psg.psg_regs[channel * 2 + 1] & 0x0Fu);
    uint16_t period = (uint16_t)(((uint16_t)hi << 8) | lo);
    return (period == 0u) ? 1u : period;
}

static uint16_t envelope_period(void)
{
    uint16_t period = (uint16_t)(((uint16_t)s_psg.psg_regs[12] << 8) | s_psg.psg_regs[11]);
    return (period == 0u) ? 1u : period;
}

static void reset_timers(void)
{
    for (int i = 0; i < 3; i++) {
        s_psg.tone_counter[i] = tone_period(i);
        s_psg.tone_level[i] = 0;
    }
    s_psg.envelope_counter = envelope_period();
    s_psg.envelope_level = 0x0Fu;
}

static int psg_init(io_config_t *config)
{
    (void)config;
    for (int i = 0; i < 16; i++) {
        s_psg.psg_regs[i] = 0;
    }
    s_psg.reg_select = 0;
    s_psg.gpio_port_a = 0;
    s_psg.gpio_port_b = 0;
    reset_timers();
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
    reset_timers();
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
            return (uint8_t)(s_psg.gpio_port_a & s_psg.psg_regs[14]);
        default:
            return (uint8_t)(s_psg.gpio_port_b & s_psg.psg_regs[15]);
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
            if ((s_psg.reg_select & 0x0Fu) <= 5u || (s_psg.reg_select & 0x0Fu) == 11u || (s_psg.reg_select & 0x0Fu) == 12u) {
                reset_timers();
            }
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
        uint32_t budget = (uint32_t)cycles;
        while (budget-- > 0u) {
            s_psg.ticks++;

            for (int ch = 0; ch < 3; ch++) {
                if (s_psg.tone_counter[ch] > 0u) {
                    s_psg.tone_counter[ch]--;
                }
                if (s_psg.tone_counter[ch] == 0u) {
                    s_psg.tone_level[ch] ^= 1u;
                    s_psg.tone_counter[ch] = tone_period(ch);
                }
            }

            if (s_psg.envelope_counter > 0u) {
                s_psg.envelope_counter--;
            }
            if (s_psg.envelope_counter == 0u) {
                s_psg.envelope_counter = envelope_period();
                if (s_psg.envelope_level > 0u) {
                    s_psg.envelope_level--;
                } else {
                    s_psg.envelope_level = 0x0Fu;
                    s_psg.irq = true;
                }
            }
        }

        s_psg.psg_regs[8] = (uint8_t)((s_psg.psg_regs[8] & 0xF0u) | (s_psg.envelope_level & 0x0Fu));
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
    .name = "st.audio_gpio.psg",
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