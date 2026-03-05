#include <stdint.h>
#include <stdbool.h>

typedef struct {
    uint32_t interface_version;
    const char *name;
    int (*init)(void *config);
    void (*reset)(void);
    void (*shutdown)(void);
    const char *(*profile_id)(void);
    uint32_t (*ram_kb)(void);
    uint32_t (*tick_hz)(void);
} machine_profile_interface_t;

static struct {
    bool initialized;
    uint32_t boots;
} s_profile;

static int profile_init(void *config)
{
    (void)config;
    s_profile.initialized = true;
    s_profile.boots++;
    return 0;
}

static void profile_reset(void)
{
}

static void profile_shutdown(void)
{
    s_profile.initialized = false;
}

static const char *profile_id(void)
{
    return "st_520_pal";
}

static uint32_t profile_ram_kb(void)
{
    return 512;
}

static uint32_t profile_tick_hz(void)
{
    return 8000000;
}

static const machine_profile_interface_t s_interface = {
    .interface_version = 0x00010000,
    .name = "st.profile.520",
    .init = profile_init,
    .reset = profile_reset,
    .shutdown = profile_shutdown,
    .profile_id = profile_id,
    .ram_kb = profile_ram_kb,
    .tick_hz = profile_tick_hz,
};

void* __attribute__((section(".text.component_entry"))) component_entry(void)
{
    return (void*)&s_interface;
}