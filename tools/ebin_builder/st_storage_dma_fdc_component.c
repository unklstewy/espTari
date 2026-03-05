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
    uint16_t transfer_remaining;
    uint16_t command_latency;
    bool drq;
    bool busy;
    uint32_t ticks;
    bool initialized;
    bool irq;
} s_dma;

enum {
    FDC_STATUS_BUSY = 0x01,
    FDC_STATUS_DRQ = 0x02,
    FDC_STATUS_INTRQ = 0x80,
};

static void recompute_fdc_status(void)
{
    s_dma.fdc_status = 0;
    if (s_dma.busy) {
        s_dma.fdc_status |= FDC_STATUS_BUSY;
    }
    if (s_dma.drq) {
        s_dma.fdc_status |= FDC_STATUS_DRQ;
    }
    if (s_dma.irq) {
        s_dma.fdc_status |= FDC_STATUS_INTRQ;
    }
}

static void start_command(uint8_t cmd)
{
    s_dma.fdc_command = cmd;
    s_dma.busy = true;
    s_dma.drq = false;
    s_dma.irq = false;
    s_dma.command_latency = (uint16_t)(64u + ((uint16_t)(cmd & 0x0Fu) * 16u));
    s_dma.transfer_remaining = (uint16_t)(256u + ((uint16_t)(s_dma.dma_mode & 0x07u) * 128u));
    recompute_fdc_status();
}

static int dma_init(io_config_t *config)
{
    (void)config;
    s_dma.dma_addr = 0;
    s_dma.dma_mode = 0;
    s_dma.fdc_command = 0;
    s_dma.fdc_status = 0;
    s_dma.transfer_remaining = 0;
    s_dma.command_latency = 0;
    s_dma.drq = false;
    s_dma.busy = false;
    s_dma.ticks = 0;
    s_dma.irq = false;
    s_dma.initialized = true;
    recompute_fdc_status();
    return 0;
}

static void dma_reset(void)
{
    s_dma.dma_addr = 0;
    s_dma.dma_mode = 0;
    s_dma.fdc_command = 0;
    s_dma.fdc_status = 0;
    s_dma.transfer_remaining = 0;
    s_dma.command_latency = 0;
    s_dma.drq = false;
    s_dma.busy = false;
    s_dma.ticks = 0;
    s_dma.irq = false;
    recompute_fdc_status();
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
            start_command(val);
            break;
        default:
            s_dma.fdc_status = val;
            break;
    }
    if ((val & 0x80u) != 0u) {
        s_dma.irq = true;
    }
    recompute_fdc_status();
}

static void dma_write_word(uint32_t addr, uint16_t val)
{
    dma_write_byte(addr, (uint8_t)((val >> 8) & 0xFFu));
    dma_write_byte(addr + 1u, (uint8_t)(val & 0xFFu));
}

static void dma_clock(int cycles)
{
    if (cycles > 0) {
        uint32_t budget = (uint32_t)cycles;
        while (budget-- > 0u) {
            s_dma.ticks++;

            if (!s_dma.busy) {
                continue;
            }

            if (s_dma.command_latency > 0u) {
                s_dma.command_latency--;
                if (s_dma.command_latency == 0u) {
                    s_dma.drq = true;
                }
                continue;
            }

            if (s_dma.transfer_remaining > 0u && (s_dma.ticks & 0x07u) == 0u) {
                s_dma.transfer_remaining--;
                s_dma.dma_addr++;
            }

            if (s_dma.transfer_remaining == 0u) {
                s_dma.busy = false;
                s_dma.drq = false;
                s_dma.irq = true;
            }
        }

        recompute_fdc_status();
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
    recompute_fdc_status();
}

static void dma_set_bus(bus_interface_t *bus)
{
    (void)bus;
}

static bool dma_bus_held(void)
{
    return s_dma.busy;
}

static const io_interface_t s_interface = {
    .interface_version = 0x00010000,
    .name = "st.storage.dma_fdc",
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