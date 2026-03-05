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
    uint32_t dma_addr;
    uint8_t dma_mode;
    uint8_t fdc_command;
    uint8_t fdc_status;
    uint32_t ticks;
    bool initialized;
    bool irq;
} s_dma;

static int dma_init(io_config_t *config)
{
    (void)config;
    s_dma.dma_addr = 0;
    s_dma.dma_mode = 0;
    s_dma.fdc_command = 0;
    s_dma.fdc_status = 0;
    s_dma.ticks = 0;
    s_dma.irq = false;
    s_dma.initialized = true;
    return 0;
}

static void dma_reset(void)
{
    s_dma.dma_addr = 0;
    s_dma.dma_mode = 0;
    s_dma.fdc_command = 0;
    s_dma.fdc_status = 0;
    s_dma.ticks = 0;
    s_dma.irq = false;
}

static void dma_shutdown(void)
{
    s_dma.initialized = false;
}

static uint8_t dma_read_byte(uint32_t addr)
{
    switch (addr & 0x07u) {
        case 0: return (uint8_t)((s_dma.dma_addr >> 16) & 0xFFu);
        case 1: return (uint8_t)((s_dma.dma_addr >> 8) & 0xFFu);
        case 2: return (uint8_t)(s_dma.dma_addr & 0xFFu);
        case 3: return s_dma.dma_mode;
        case 4: return s_dma.fdc_command;
        default: return s_dma.fdc_status;
    }
}

static uint16_t dma_read_word(uint32_t addr)
{
    uint8_t hi = dma_read_byte(addr);
    uint8_t lo = dma_read_byte(addr + 1u);
    return (uint16_t)((hi << 8) | lo);
}

static void dma_write_byte(uint32_t addr, uint8_t val)
{
    switch (addr & 0x07u) {
        case 0:
            s_dma.dma_addr = (s_dma.dma_addr & 0x00FFFFu) | ((uint32_t)val << 16);
            break;
        case 1:
            s_dma.dma_addr = (s_dma.dma_addr & 0xFF00FFu) | ((uint32_t)val << 8);
            break;
        case 2:
            s_dma.dma_addr = (s_dma.dma_addr & 0xFFFF00u) | (uint32_t)val;
            break;
        case 3:
            s_dma.dma_mode = val;
            break;
        case 4:
            s_dma.fdc_command = val;
            s_dma.fdc_status = 0x01u;
            break;
        default:
            s_dma.fdc_status = val;
            break;
    }
    if ((val & 0x80u) != 0u) {
        s_dma.irq = true;
    }
}

static void dma_write_word(uint32_t addr, uint16_t val)
{
    dma_write_byte(addr, (uint8_t)((val >> 8) & 0xFFu));
    dma_write_byte(addr + 1u, (uint8_t)(val & 0xFFu));
}

static void dma_clock(int cycles)
{
    if (cycles > 0) {
        s_dma.ticks += (uint32_t)cycles;
        if ((s_dma.ticks % 1024u) == 0u) {
            s_dma.fdc_status |= 0x80u;
            s_dma.irq = true;
        }
    }
}

static bool dma_irq_pending(void)
{
    return s_dma.irq;
}

static uint8_t dma_get_vector(void)
{
    return 0x4C;
}

static void dma_irq_ack(void)
{
    s_dma.irq = false;
    s_dma.fdc_status &= (uint8_t)~0x80u;
}

static void dma_set_bus(bus_interface_t *bus)
{
    (void)bus;
}

static bool dma_bus_held(void)
{
    return false;
}

static const io_interface_t s_interface = {
    .interface_version = 0x00010000,
    .name = "st.storage.dma_fdc.stub",
    .init = dma_init,
    .reset = dma_reset,
    .shutdown = dma_shutdown,
    .read_byte = dma_read_byte,
    .read_word = dma_read_word,
    .write_byte = dma_write_byte,
    .write_word = dma_write_word,
    .clock = dma_clock,
    .irq_pending = dma_irq_pending,
    .get_vector = dma_get_vector,
    .irq_ack = dma_irq_ack,
    .set_bus = dma_set_bus,
    .bus_held = dma_bus_held,
};

void* __attribute__((section(".text.component_entry"))) component_entry(void)
{
    return (void*)&s_interface;
}