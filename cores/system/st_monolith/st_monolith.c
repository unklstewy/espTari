/**
 * @file st_monolith.c
 * @brief Unified monolithic ST system EBIN prototype
 *
 * This is an integration scaffold for a single-EBIN machine artifact. It
 * intentionally starts as a minimal contract implementation so loader/runtime
 * plumbing can be validated before full CPU/memory/video/audio unification.
 *
 * SPDX-License-Identifier: MIT
 */

#include "component_api.h"

static int monolith_init(void *config)
{
    (void)config;
    return 0;
}

static void monolith_reset(void)
{
}

static void monolith_shutdown(void)
{
}

static cpu_interface_t *monolith_get_cpu(void)
{
    return NULL;
}

static video_interface_t *monolith_get_video(void)
{
    return NULL;
}

static audio_interface_t *monolith_get_audio(int index)
{
    (void)index;
    return NULL;
}

static io_interface_t *monolith_get_io(int index)
{
    (void)index;
    return NULL;
}

static system_interface_t s_system_interface = {
    .interface_version = SYSTEM_INTERFACE_V1,
    .name = "ST Monolith Prototype",
    .init = monolith_init,
    .reset = monolith_reset,
    .shutdown = monolith_shutdown,
    .get_cpu = monolith_get_cpu,
    .get_video = monolith_get_video,
    .get_audio = monolith_get_audio,
    .get_io = monolith_get_io,
};

void *component_entry(void)
{
    return &s_system_interface;
}
