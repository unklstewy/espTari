/**
 * @file test_component.c
 * @brief Simple test component for EBIN loader validation
 * 
 * This is a minimal I/O component that can be loaded to verify
 * the EBIN build and load pipeline works correctly.
 * 
 * SPDX-License-Identifier: MIT
 */

#include <stdint.h>
#include <stdbool.h>

/*===========================================================================*/
/* Minimal interface definitions (no external headers for PIC)               */
/*===========================================================================*/

typedef struct bus_interface_s bus_interface_t;

/* IO config */
typedef struct {
    void *context;
} io_config_t;

/* IO interface - matches component_api.h io_interface_t */
typedef struct {
    uint32_t    interface_version;
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

/*===========================================================================*/
/* Component State                                                           */
/*===========================================================================*/

static struct {
    uint8_t  registers[256];
    uint32_t access_count;
    bool     initialized;
} s_state;

/*===========================================================================*/
/* Interface Implementation                                                  */
/*===========================================================================*/

static int test_init(io_config_t *config)
{
    (void)config;
    for (int i = 0; i < 256; i++) {
        s_state.registers[i] = 0;
    }
    s_state.access_count = 0;
    s_state.initialized = true;
    return 0;  /* ESP_OK */
}

static void test_shutdown(void)
{
    s_state.initialized = false;
}

static void test_reset(void)
{
    for (int i = 0; i < 256; i++) {
        s_state.registers[i] = 0;
    }
    s_state.access_count = 0;
}

static void* test_get_state(void)
{
    return &s_state;
}

static void test_set_state(const void *state)
{
    const uint8_t *src = (const uint8_t*)state;
    uint8_t *dst = (uint8_t*)&s_state;
    for (unsigned i = 0; i < sizeof(s_state); i++) {
        dst[i] = src[i];
    }
}

static uint32_t test_state_size(void)
{
    return sizeof(s_state);
}

static uint8_t test_read_byte(uint32_t addr)
{
    s_state.access_count++;
    return s_state.registers[addr & 0xFF];
}

static uint16_t test_read_word(uint32_t addr)
{
    s_state.access_count++;
    uint8_t hi = s_state.registers[addr & 0xFF];
    uint8_t lo = s_state.registers[(addr + 1) & 0xFF];
    return (hi << 8) | lo;
}

static void test_write_byte(uint32_t addr, uint8_t val)
{
    s_state.access_count++;
    s_state.registers[addr & 0xFF] = val;
}

static void test_write_word(uint32_t addr, uint16_t val)
{
    s_state.access_count++;
    s_state.registers[addr & 0xFF] = (val >> 8) & 0xFF;
    s_state.registers[(addr + 1) & 0xFF] = val & 0xFF;
}

static void test_clock(int cycles)
{
    (void)cycles;
    /* No timing behavior for test component */
}

static bool test_irq_pending(void)
{
    return false;
}

static uint8_t test_get_vector(void)
{
    return 0;
}

static void test_irq_ack(void)
{
    /* No-op */
}

static void test_set_bus(bus_interface_t *bus)
{
    (void)bus;
}

static bool test_bus_held(void)
{
    return false;
}

/*===========================================================================*/
/* Interface Definition                                                      */
/*===========================================================================*/

static const io_interface_t s_interface = {
    .interface_version = 0x00010000,  /* IO_INTERFACE_V1 */
    .name              = "TestIO",
    
    .init        = test_init,
    .reset       = test_reset,
    .shutdown    = test_shutdown,
    
    .read_byte   = test_read_byte,
    .read_word   = test_read_word,
    .write_byte  = test_write_byte,
    .write_word  = test_write_word,
    
    .clock       = test_clock,
    
    .irq_pending = test_irq_pending,
    .get_vector  = test_get_vector,
    .irq_ack     = test_irq_ack,
    
    .set_bus     = test_set_bus,
    .bus_held    = test_bus_held,
};

/*===========================================================================*/
/* Entry Point                                                               */
/*===========================================================================*/

/**
 * @brief Component entry point
 * 
 * Called by loader after relocations are applied.
 * Returns pointer to the interface structure.
 */
void* __attribute__((section(".text.component_entry"))) component_entry(void)
{
    return (void*)&s_interface;
}
