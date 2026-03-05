#include <stdint.h>
#include <stdbool.h>

typedef struct bus_interface_s bus_interface_t;

typedef struct {
    uint32_t clock_hz;
    void *context;
} cpu_config_t;

typedef struct {
    uint32_t d[8];
    uint32_t a[8];
    uint32_t pc;
    uint16_t sr;
} cpu_state_t;

typedef struct {
    uint32_t interface_version;
    const char *name;
    uint32_t features;

    int  (*init)(cpu_config_t *config);
    void (*reset)(void);
    void (*shutdown)(void);
    void (*set_bus)(bus_interface_t *bus);
    int  (*execute)(int cycles);
    void (*set_irq)(uint8_t level);
    void (*get_state)(cpu_state_t *out_state);
    void (*set_state)(const cpu_state_t *in_state);
} cpu_interface_t;

static struct {
    cpu_state_t state;
    uint32_t steps_total;
    uint8_t pending_irq_level;
    bool halted;
    bool initialized;
    bus_interface_t *bus;
} s_cpu;

static uint16_t sr_with_ipl(uint16_t sr, uint8_t level)
{
    return (uint16_t)((sr & 0xF8FFu) | ((uint16_t)(level & 0x07u) << 8));
}

static void cpu_take_pending_irq(void)
{
    uint8_t current_ipl = (uint8_t)((s_cpu.state.sr >> 8) & 0x07u);
    if (s_cpu.pending_irq_level <= current_ipl) {
        return;
    }

    s_cpu.state.sr = sr_with_ipl(s_cpu.state.sr, s_cpu.pending_irq_level);
    s_cpu.state.pc = (uint32_t)(0x00000018u + ((uint32_t)s_cpu.pending_irq_level * 4u));
    s_cpu.pending_irq_level = 0;
    s_cpu.halted = false;
}

static int cpu_init(cpu_config_t *config)
{
    (void)config;
    for (int i = 0; i < 8; i++) {
        s_cpu.state.d[i] = 0;
        s_cpu.state.a[i] = 0;
    }
    s_cpu.state.pc = 0;
    s_cpu.state.sr = 0x2000;
    s_cpu.state.a[7] = 0x00040000u;
    s_cpu.steps_total = 0;
    s_cpu.pending_irq_level = 0;
    s_cpu.halted = false;
    s_cpu.initialized = true;
    return 0;
}

static void cpu_reset(void)
{
    s_cpu.state.pc = 0;
    s_cpu.state.sr = 0x2000;
    s_cpu.state.a[7] = 0x00040000u;
    s_cpu.steps_total = 0;
    s_cpu.pending_irq_level = 0;
    s_cpu.halted = false;
}

static void cpu_shutdown(void)
{
    s_cpu.initialized = false;
    s_cpu.pending_irq_level = 0;
    s_cpu.halted = false;
    s_cpu.bus = (bus_interface_t *)0;
}

static void cpu_set_bus(bus_interface_t *bus)
{
    s_cpu.bus = bus;
}

static int cpu_execute(int cycles)
{
    if (!s_cpu.initialized || cycles <= 0) {
        return 0;
    }

    uint32_t budget = (uint32_t)cycles;
    uint32_t consumed = 0;

    while (consumed < budget) {
        cpu_take_pending_irq();

        if (s_cpu.halted) {
            consumed += 2;
            continue;
        }

        s_cpu.state.pc += 2u;
        s_cpu.steps_total++;
        consumed += 4;

        if ((s_cpu.steps_total & 0x1FFFu) == 0u) {
            s_cpu.halted = true;
        }
    }

    return (int)consumed;
}

static void cpu_set_irq(uint8_t level)
{
    uint8_t clamped = (uint8_t)(level & 0x07u);
    if (clamped > s_cpu.pending_irq_level) {
        s_cpu.pending_irq_level = clamped;
    }
}

static void cpu_get_state(cpu_state_t *out_state)
{
    if (out_state == (cpu_state_t *)0) {
        return;
    }
    *out_state = s_cpu.state;
}

static void cpu_set_state(const cpu_state_t *in_state)
{
    if (in_state == (const cpu_state_t *)0) {
        return;
    }
    s_cpu.state = *in_state;
}

static const cpu_interface_t s_interface = {
    .interface_version = 0x00010000,
    .name = "st.cpu.m68k",
    .features = 0x00000003,
    .init = cpu_init,
    .reset = cpu_reset,
    .shutdown = cpu_shutdown,
    .set_bus = cpu_set_bus,
    .execute = cpu_execute,
    .set_irq = cpu_set_irq,
    .get_state = cpu_get_state,
    .set_state = cpu_set_state,
};

void* __attribute__((section(".text.component_entry"))) component_entry(void)
{
    return (void*)&s_interface;
}