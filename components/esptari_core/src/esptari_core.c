/**
 * @file esptari_core.c
 * @brief Core emulation framework — machine lifecycle, timing, component bus
 *
 * SPDX-License-Identifier: MIT
 */
#include "esptari_core.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "component_api.h"
#include "esp_timer.h"
#include "esptari_memory.h"
#include "st_glue.h"
#include "st_acia.h"
#include "esptari_video.h"
#include "machine.h"
#include "esptari_loader.h"
#include <string.h>
#include <inttypes.h>
#include <stdio.h>
#include <sys/stat.h>

static const char *TAG = "esptari_core";

static esptari_state_t s_state = ESPTARI_STATE_STOPPED;
static esptari_machine_t s_machine = ESPTARI_MACHINE_ST;
static TaskHandle_t s_emu_task = NULL;
static bool s_core_ready = false;
static bool s_stop_requested = false;
static uint32_t s_loop_stall_samples = 0;
static bool s_loop_trace_captured = false;

#define LOOP_TRACE_PATH "/sdcard/logs/stacktrace.txt"
#define LOOP_TRACE_WORDS 256U
#define LOOP_STALL_MIN_SAMPLES 3U

static cpu_interface_t *s_cpu = NULL;
static io_interface_t *s_mfp = NULL;
static video_interface_t *s_video = NULL;
static audio_interface_t *s_audio = NULL;
static system_interface_t *s_system = NULL;
static int s_pending_mfp_vector = -1;

extern void *m68000_entry(void);
extern void *shifter_entry(void);
extern void *ym2149_entry(void);
extern void *mfp68901_entry(void);
extern void m68k_set_level6_vector(int vector);
extern int m68k_get_microtrace_text(char *buf, int buf_size);

/*---------------------------------------------------------------------------*/
/* I/O Bridges                                                               */
/*---------------------------------------------------------------------------*/

static uint8_t mfp_io_bridge_read_byte(uint32_t addr, void *ctx)
{
    io_interface_t *mfp = (io_interface_t *)ctx;
    return mfp->read_byte(addr);
}

static uint16_t mfp_io_bridge_read_word(uint32_t addr, void *ctx)
{
    io_interface_t *mfp = (io_interface_t *)ctx;
    return mfp->read_word(addr);
}

static void mfp_io_bridge_write_byte(uint32_t addr, uint8_t val, void *ctx)
{
    io_interface_t *mfp = (io_interface_t *)ctx;
    mfp->write_byte(addr, val);
}

static void mfp_io_bridge_write_word(uint32_t addr, uint16_t val, void *ctx)
{
    io_interface_t *mfp = (io_interface_t *)ctx;
    mfp->write_word(addr, val);
}

static uint8_t video_io_bridge_read_byte(uint32_t addr, void *ctx)
{
    video_interface_t *video = (video_interface_t *)ctx;
    uint16_t word = video->read_reg(addr & ~1U);
    return (addr & 1U) ? (uint8_t)(word & 0xFF) : (uint8_t)(word >> 8);
}

static uint16_t video_io_bridge_read_word(uint32_t addr, void *ctx)
{
    video_interface_t *video = (video_interface_t *)ctx;
    return video->read_reg(addr);
}

static void video_io_bridge_write_byte(uint32_t addr, uint8_t val, void *ctx)
{
    video_interface_t *video = (video_interface_t *)ctx;
    uint32_t aligned = addr & ~1U;
    uint16_t current = video->read_reg(aligned);
    uint16_t merged;

    if (addr & 1U) {
        merged = (uint16_t)((current & 0xFF00U) | val);
    } else {
        merged = (uint16_t)((current & 0x00FFU) | ((uint16_t)val << 8));
    }

    video->write_reg(aligned, merged);
}

static void video_io_bridge_write_word(uint32_t addr, uint16_t val, void *ctx)
{
    video_interface_t *video = (video_interface_t *)ctx;
    video->write_reg(addr, val);
}

static uint8_t audio_io_bridge_read_byte(uint32_t addr, void *ctx)
{
    audio_interface_t *audio = (audio_interface_t *)ctx;
    return audio->read_reg(addr);
}

static uint16_t audio_io_bridge_read_word(uint32_t addr, void *ctx)
{
    audio_interface_t *audio = (audio_interface_t *)ctx;
    return 0xFF00 | audio->read_reg(addr);
}

static void audio_io_bridge_write_byte(uint32_t addr, uint8_t val, void *ctx)
{
    audio_interface_t *audio = (audio_interface_t *)ctx;
    audio->write_reg(addr, val);
}

static void audio_io_bridge_write_word(uint32_t addr, uint16_t val, void *ctx)
{
    audio_interface_t *audio = (audio_interface_t *)ctx;
    audio->write_reg(addr, (uint8_t)(val & 0xFF));
}

static void glue_mfp_clock_bridge(int cycles)
{
    if (s_mfp && s_mfp->clock) {
        s_mfp->clock(cycles);
    }
}

static bool glue_mfp_irq_bridge(void)
{
    if (s_mfp && s_mfp->irq_pending) {
        bool pending = s_mfp->irq_pending();
        if (pending && s_mfp->get_vector) {
            s_pending_mfp_vector = s_mfp->get_vector();
        }
        return pending;
    }
    return false;
}

static void update_level6_vector_from_mfp(void)
{
    if (s_pending_mfp_vector < 0) {
        return;
    }
    m68k_set_level6_vector(s_pending_mfp_vector);
    s_pending_mfp_vector = -1;
}

static void ram_write_be32(uint8_t *ram, uint32_t ram_size, uint32_t addr, uint32_t value)
{
    if (!ram || (addr + 3U) >= ram_size) {
        return;
    }

    ram[addr + 0U] = (uint8_t)(value >> 24);
    ram[addr + 1U] = (uint8_t)(value >> 16);
    ram[addr + 2U] = (uint8_t)(value >> 8);
    ram[addr + 3U] = (uint8_t)(value);
}

/*---------------------------------------------------------------------------*/
/* Core Helpers                                                              */
/*---------------------------------------------------------------------------*/

static void setup_reset_vectors(void)
{
    uint8_t *ram = st_memory_get_ram();
    uint8_t *rom = st_memory_get_rom();
    uint32_t rom_size = st_memory_get_rom_size();
    uint32_t ram_size = st_memory_get_ram_size();

    if (!ram || !rom || rom_size < 8 || ram_size < 8) {
        return;
    }

    uint32_t rom_reset_ssp = ((uint32_t)rom[0] << 24) |
                             ((uint32_t)rom[1] << 16) |
                             ((uint32_t)rom[2] << 8) |
                             (uint32_t)rom[3];
    uint32_t rom_reset_pc = ((uint32_t)rom[4] << 24) |
                            ((uint32_t)rom[5] << 16) |
                            ((uint32_t)rom[6] << 8) |
                            (uint32_t)rom[7];

    bool rom_looks_vectorless = ((rom[0] & 0xF0U) == 0x60U) || (rom_reset_ssp >= ram_size);

    if (!rom_looks_vectorless) {
        uint32_t copy_len = 256U * 4U; /* 68000 vector table (vectors 0-255) */
        if (copy_len > rom_size) {
            copy_len = rom_size;
        }
        if (copy_len > ram_size) {
            copy_len = ram_size;
        }
        memcpy(ram, rom, copy_len);
        return;
    }

    const uint32_t MEMVALID_ADDR = 0x000420U;
    const uint32_t PHYSTOP_ADDR = 0x00042EU;
    const uint32_t MEMBOT_ADDR = 0x000432U;
    const uint32_t MEMTOP_ADDR = 0x000436U;
    const uint32_t MEMVALID2_ADDR = 0x00043AU;

    uint32_t reset_ssp = (ram_size >= 4U) ? ((ram_size - 4U) & ~1U) : 0U;
    uint32_t reset_pc = rom_reset_pc & 0x00FFFFFFU;
    uint32_t rom_base = ST_ROM_BASE;
    uint32_t rom_end = ST_ROM_BASE + rom_size;

    if (reset_pc < rom_base || reset_pc >= rom_end) {
        reset_pc = ST_ROM_BASE;
    }

    uint32_t vector_table_bytes = 256U * 4U;
    if (vector_table_bytes > ram_size) {
        vector_table_bytes = ram_size;
    }

    for (uint32_t off = 0; (off + 3U) < vector_table_bytes; off += 4U) {
        ram[off + 0U] = (uint8_t)(reset_pc >> 24);
        ram[off + 1U] = (uint8_t)(reset_pc >> 16);
        ram[off + 2U] = (uint8_t)(reset_pc >> 8);
        ram[off + 3U] = (uint8_t)(reset_pc);
    }

    ram[0] = (uint8_t)(reset_ssp >> 24);
    ram[1] = (uint8_t)(reset_ssp >> 16);
    ram[2] = (uint8_t)(reset_ssp >> 8);
    ram[3] = (uint8_t)(reset_ssp);

    ram[4] = (uint8_t)(reset_pc >> 24);
    ram[5] = (uint8_t)(reset_pc >> 16);
    ram[6] = (uint8_t)(reset_pc >> 8);
    ram[7] = (uint8_t)(reset_pc);

    ram_write_be32(ram, ram_size, MEMVALID_ADDR, 0x752019F3U);
    ram_write_be32(ram, ram_size, PHYSTOP_ADDR, ram_size);
    ram_write_be32(ram, ram_size, MEMBOT_ADDR, 0x00000800U);
    ram_write_be32(ram, ram_size, MEMTOP_ADDR, ram_size);
    ram_write_be32(ram, ram_size, MEMVALID2_ADDR, 0x237698AAU);

    ESP_LOGW(TAG,
             "Vectorless ROM detected (hdr=$%04X), synthesized reset SSP=$%08" PRIX32 " PC=$%08" PRIX32,
             ((uint16_t)rom[0] << 8) | rom[1],
             reset_ssp,
             reset_pc);
}

static const char *machine_default_tos(esptari_machine_t machine)
{
    switch (machine) {
        case ESPTARI_MACHINE_ST:
        case ESPTARI_MACHINE_STFM:
            return "/sdcard/roms/tos/tos104.img";
        case ESPTARI_MACHINE_MEGA_ST:
        case ESPTARI_MACHINE_STE:
        case ESPTARI_MACHINE_MEGA_STE:
            return "/sdcard/roms/tos/tos206.img";
        case ESPTARI_MACHINE_TT030:
            return "/sdcard/roms/tos/tos306.img";
        case ESPTARI_MACHINE_FALCON030:
            return "/sdcard/roms/tos/tos404.img";
        default:
            return "/sdcard/roms/tos/tos104.img";
    }
}

static void try_load_rom_or_continue(void)
{
    const char *preferred = machine_default_tos(s_machine);
    const char *candidates[] = {
        preferred,
        "/sdcard/roms/tos/tos104us.img",
        "/sdcard/roms/tos/tos104.img",
        "/sdcard/roms/tos/tos206.img",
        "/sdcard/roms/tos/etos1024k.img",
    };

    for (size_t i = 0; i < (sizeof(candidates) / sizeof(candidates[0])); i++) {
        if (st_memory_load_rom(candidates[i]) == ESP_OK) {
            ESP_LOGI(TAG, "Loaded TOS ROM: %s", candidates[i]);
            return;
        }
    }

    ESP_LOGW(TAG, "No TOS ROM found, continuing with zeroed RAM vectors");
}

static esptari_resolution_t detect_resolution(void)
{
    video_mode_t mode;
    memset(&mode, 0, sizeof(mode));

    if (!s_video || !s_video->get_mode) {
        return ESPTARI_RES_LOW;
    }

    s_video->get_mode(&mode);
    if (mode.width == 320 && mode.height == 200) {
        return ESPTARI_RES_LOW;
    }
    if (mode.width == 640 && mode.height == 200) {
        return ESPTARI_RES_MED;
    }
    return ESPTARI_RES_HIGH;
}

static void render_scaled_frame(uint16_t *dst)
{
    if (!s_video || !s_video->render_scanline || !dst) {
        return;
    }

    esptari_resolution_t res = detect_resolution();

    if (res == ESPTARI_RES_HIGH) {
        for (int y = 0; y < 400; y++) {
            s_video->render_scanline(y, dst + (y * 640));
            if ((y & 0x0F) == 0) {
                taskYIELD();
            }
        }
        return;
    }

    if (res == ESPTARI_RES_MED) {
        uint16_t line[640];
        for (int y = 0; y < 200; y++) {
            uint16_t *d0 = dst + ((y * 2) * 640);
            uint16_t *d1 = d0 + 640;
            s_video->render_scanline(y, line);
            memcpy(d0, line, sizeof(line));
            memcpy(d1, line, sizeof(line));
            if ((y & 0x0F) == 0) {
                taskYIELD();
            }
        }
        return;
    }

    uint16_t line[320];
    for (int y = 0; y < 200; y++) {
        uint16_t *d0 = dst + ((y * 2) * 640);
        uint16_t *d1 = d0 + 640;

        s_video->render_scanline(y, line);
        for (int x = 0; x < 320; x++) {
            uint16_t px = line[x];
            int dx = x * 2;
            d0[dx] = px;
            d0[dx + 1] = px;
            d1[dx] = px;
            d1[dx + 1] = px;
        }

        if ((y & 0x0F) == 0) {
            taskYIELD();
        }
    }
}

static bool frame_is_uniform_sampled(const uint16_t *frame)
{
    if (!frame) {
        return true;
    }

    const uint16_t first = frame[0];
    const int total = ESPTARI_VIDEO_MAX_WIDTH * ESPTARI_VIDEO_MAX_HEIGHT;
    const int step = 977;

    for (int i = step; i < total; i += step) {
        if (frame[i] != first) {
            return false;
        }
    }
    return true;
}

static esp_err_t setup_static_components(void)
{
    bool using_loaded_machine = false;
    bool using_unified_system = false;
    bool monolith_required = false;

    if (s_core_ready) {
        return ESP_OK;
    }

    s_cpu = NULL;
    s_video = NULL;
    s_audio = NULL;
    s_mfp = NULL;
    s_system = NULL;

    if (esptari_loader_unified_enabled()) {
        const char *resolved_profile = esptari_loader_get_resolved_profile_name();
        if (resolved_profile && strstr(resolved_profile, "monolith") != NULL) {
            monolith_required = true;
            ESP_LOGI(TAG, "Monolith profile required: %s", resolved_profile);
        }
    }

    machine_state_t *loaded_machine = machine_get_state();
    if (loaded_machine) {
        s_system = machine_get_system();
        if (s_system) {
            using_loaded_machine = true;
            using_unified_system = true;

            if (s_system->init && s_system->init(NULL) != 0) {
                ESP_LOGE(TAG, "Unified system init failed");
                return ESP_FAIL;
            }

            if (s_system->get_cpu) {
                s_cpu = s_system->get_cpu();
            }
            if (s_system->get_video) {
                s_video = s_system->get_video();
            }
            if (s_system->get_audio) {
                s_audio = s_system->get_audio(0);
            }
            if (s_system->get_io) {
                s_mfp = s_system->get_io(0);
            }
        } else {
            using_loaded_machine = true;
            s_cpu = machine_get_cpu();
            s_video = machine_get_video();
            s_audio = machine_get_audio(0);
            s_mfp = machine_get_io(0);
        }
    }

    if (!using_loaded_machine) {
        if (monolith_required) {
            ESP_LOGE(TAG,
                     "Monolith profile selected but no loaded machine/system interface available");
            return ESP_ERR_INVALID_STATE;
        }

        s_cpu = (cpu_interface_t *)m68000_entry();
        s_video = (video_interface_t *)shifter_entry();
        s_audio = (audio_interface_t *)ym2149_entry();
        s_mfp = (io_interface_t *)mfp68901_entry();

        if (!s_cpu || !s_video || !s_audio || !s_mfp) {
            ESP_LOGE(TAG, "Static core component lookup failed");
            return ESP_FAIL;
        }

        ESP_LOGI(TAG, "Core using static built-in components");
    } else if (!s_cpu || !s_video || !s_audio || !s_mfp) {
        ESP_LOGE(TAG,
                 "Loaded machine missing required interfaces cpu=%p video=%p audio0=%p io0=%p",
                 (void *)s_cpu,
                 (void *)s_video,
                 (void *)s_audio,
                 (void *)s_mfp);
        return ESP_ERR_INVALID_STATE;
    } else if (using_unified_system) {
        ESP_LOGI(TAG, "Core using unified system interfaces: %s",
                 s_system->name ? s_system->name : "unnamed");
    } else {
        ESP_LOGI(TAG, "Core using loaded discrete machine components");
    }

    esp_err_t ret = st_memory_init(ST_RAM_DEFAULT);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "st_memory_init failed: %s", esp_err_to_name(ret));
        return ret;
    }

    try_load_rom_or_continue();

    st_glue_init(true);
    st_acia_init();

    cpu_config_t cpu_cfg = {
        .clock_hz = ST_CPU_CLOCK_HZ,
        .context = NULL,
    };
    video_config_t video_cfg = {
        .framebuffer = NULL,
        .fb_size = 0,
        .context = NULL,
    };
    audio_config_t audio_cfg = {
        .sample_rate = 44100,
        .context = NULL,
    };
    io_config_t io_cfg = {
        .context = NULL,
    };

    if (!using_unified_system) {
        if (s_mfp->init) s_mfp->init(&io_cfg);
        if (s_video->init) s_video->init(&video_cfg);
        if (s_audio->init) s_audio->init(&audio_cfg);
        if (s_cpu->init) s_cpu->init(&cpu_cfg);
    }

    bus_interface_t *bus = st_memory_get_bus();
    if (!bus) {
        ESP_LOGE(TAG, "Failed to get ST bus interface");
        return ESP_FAIL;
    }

    if (s_cpu->set_bus) s_cpu->set_bus(bus);
    if (s_video->set_bus) s_video->set_bus(bus);
    if (s_audio->set_bus) s_audio->set_bus(bus);
    if (s_mfp->set_bus) s_mfp->set_bus(bus);

    io_handler_t mfp_handler = {
        .base = IO_MFP_BASE,
        .end = IO_MFP_END,
        .read_byte = mfp_io_bridge_read_byte,
        .read_word = mfp_io_bridge_read_word,
        .write_byte = mfp_io_bridge_write_byte,
        .write_word = mfp_io_bridge_write_word,
        .context = s_mfp,
        .name = "MFP 68901",
    };
    io_handler_t video_handler = {
        .base = IO_VIDEO_BASE,
        .end = IO_VIDEO_END,
        .read_byte = video_io_bridge_read_byte,
        .read_word = video_io_bridge_read_word,
        .write_byte = video_io_bridge_write_byte,
        .write_word = video_io_bridge_write_word,
        .context = s_video,
        .name = "Shifter",
    };
    io_handler_t audio_handler = {
        .base = IO_PSG_BASE,
        .end = IO_PSG_END,
        .read_byte = audio_io_bridge_read_byte,
        .read_word = audio_io_bridge_read_word,
        .write_byte = audio_io_bridge_write_byte,
        .write_word = audio_io_bridge_write_word,
        .context = s_audio,
        .name = "YM2149",
    };

    st_memory_register_io(&mfp_handler);
    st_memory_register_io(&video_handler);
    st_memory_register_io(&audio_handler);

    st_glue_connect_cpu(s_cpu->set_irq);
    st_glue_connect_mfp(glue_mfp_clock_bridge, glue_mfp_irq_bridge);

    st_memory_reset();
    setup_reset_vectors();
    st_glue_reset();
    if (s_mfp->reset) s_mfp->reset();
    if (s_video->reset) s_video->reset();
    if (s_audio->reset) s_audio->reset();
    if (s_cpu->reset) s_cpu->reset();

    s_core_ready = true;
    ESP_LOGI(TAG, "Static ST core wired and ready");
    return ESP_OK;
}

static void emulation_task(void *arg)
{
    (void)arg;
    TickType_t last_wake = xTaskGetTickCount();
    const TickType_t frame_ticks = pdMS_TO_TICKS(1000 / ST_FPS_PAL);
    uint32_t frames_this_sec = 0;
    int64_t last_dbg_us = esp_timer_get_time();
    uint32_t uniform_frames = 0;

    while (!s_stop_requested) {
        if (s_state == ESPTARI_STATE_PAUSED) {
            vTaskDelay(pdMS_TO_TICKS(5));
            continue;
        }

        int frame_cycles = 0;
        while (frame_cycles < ST_CYCLES_PER_FRAME && !s_stop_requested) {
            update_level6_vector_from_mfp();

            int chunk = ST_CYCLES_PER_LINE;
            int remain = ST_CYCLES_PER_FRAME - frame_cycles;
            if (chunk > remain) {
                chunk = remain;
            }

            int executed = s_cpu->execute(chunk);
            if (executed <= 0) {
                executed = chunk;
            }

            frame_cycles += executed;
            st_glue_clock(executed);

            if (s_video && s_video->clock) {
                s_video->clock(executed);
            }
            if (s_audio && s_audio->clock) {
                s_audio->clock(executed);
            }

            if ((frame_cycles & ((ST_CYCLES_PER_LINE * 16) - 1)) == 0) {
                taskYIELD();
            }
        }

        uint8_t *write_buf = NULL;
        size_t write_size = 0;
        if (esptari_video_get_write_buffer(&write_buf, &write_size) == ESP_OK &&
            write_buf && write_size >= ESPTARI_VIDEO_MAX_FRAME_SIZE) {
            render_scaled_frame((uint16_t *)write_buf);

            if (frame_is_uniform_sampled((const uint16_t *)write_buf)) {
                uniform_frames++;
            } else {
                uniform_frames = 0;
            }

            if (uniform_frames > 30) {
                esptari_video_generate_test_pattern();
            } else {
                esptari_video_swap(detect_resolution());
            }

            frames_this_sec++;
        }

        int64_t now_us = esp_timer_get_time();
        if (now_us - last_dbg_us >= 1000000) {
            cpu_state_t cpu_state;
            memset(&cpu_state, 0, sizeof(cpu_state));
            if (s_cpu && s_cpu->get_state) {
                s_cpu->get_state(&cpu_state);
            }

            video_mode_t mode;
            memset(&mode, 0, sizeof(mode));
            if (s_video && s_video->get_mode) {
                s_video->get_mode(&mode);
            }

            uint32_t video_base = 0;
            uint16_t pal0 = 0;
            uint16_t res = 0;
            uint8_t mmu_cfg = 0xFF;
            uint8_t mfp_vr = 0xFF;
            uint8_t mfp_ipra = 0xFF;
            uint8_t mfp_iprb = 0xFF;
            uint8_t mfp_imra = 0xFF;
            uint8_t mfp_imrb = 0xFF;
            uint64_t mem_reads = 0;
            uint64_t mem_writes = 0;
            uint64_t mem_bus_errors = 0;
            uint32_t last_bus_error_addr = 0;
            bool last_bus_error_write = false;
            uint16_t opcode = 0;
            uint16_t opcode1 = 0;
            uint16_t opcode2 = 0;
            st_acia_debug_t acia_dbg;
            memset(&acia_dbg, 0, sizeof(acia_dbg));
            bus_interface_t *bus = st_memory_get_bus();
            if (bus && bus->read_byte && bus->read_word) {
                uint8_t hi = bus->read_byte(0xFF8201);
                uint8_t mid = bus->read_byte(0xFF8203);
                uint8_t lo = bus->read_byte(0xFF820D);
                video_base = ((uint32_t)(hi & 0x3F) << 16) | ((uint32_t)mid << 8) | (lo & 0xFE);
                pal0 = bus->read_word(0xFF8240);
                res = bus->read_byte(0xFF8260) & 0x03;
                mmu_cfg = bus->read_byte(0xFF8001);
                mfp_vr = bus->read_byte(0xFFFA17);
                mfp_ipra = bus->read_byte(0xFFFA0B);
                mfp_iprb = bus->read_byte(0xFFFA0D);
                mfp_imra = bus->read_byte(0xFFFA13);
                mfp_imrb = bus->read_byte(0xFFFA15);
                opcode = bus->read_word(cpu_state.pc & 0x00FFFFFE);
                opcode1 = bus->read_word((cpu_state.pc + 2U) & 0x00FFFFFE);
                opcode2 = bus->read_word((cpu_state.pc + 4U) & 0x00FFFFFE);
            }
            st_memory_get_stats(&mem_reads, &mem_writes, &mem_bus_errors);
            st_memory_get_last_bus_error(&last_bus_error_addr, &last_bus_error_write);
            st_acia_get_debug(&acia_dbg);

            bool loop_stall_candidate =
                (cpu_state.pc >= 0x00FC01C0U && cpu_state.pc <= 0x00FC01D0U) &&
                (cpu_state.d[4] == 0x00000400U) &&
                (opcode == 0x48E0U || opcode == 0xF000U || opcode == 0xB1C4U || opcode == 0x66ECU || opcode == 0x9BCDU);

            if (loop_stall_candidate) {
                s_loop_stall_samples++;
            } else {
                s_loop_stall_samples = 0;
            }

            if (!s_loop_trace_captured && s_loop_stall_samples >= LOOP_STALL_MIN_SAMPLES) {
                esp_err_t dump_ret = esptari_core_dump_stacktrace(LOOP_TRACE_PATH, LOOP_TRACE_WORDS);
                if (dump_ret == ESP_OK) {
                    s_loop_trace_captured = true;
                    ESP_LOGW(TAG,
                             "Auto stacktrace captured after loop stall (%lu samples): %s",
                             (unsigned long)s_loop_stall_samples,
                             LOOP_TRACE_PATH);
                } else {
                    ESP_LOGW(TAG,
                             "Auto stacktrace capture failed: %s",
                             esp_err_to_name(dump_ret));
                }
            }

            ESP_LOGI(TAG,
                     "EMU fps=%lu pc=$%06" PRIX32 " op=$%04X/%04X/%04X sr=$%04X irq=%d h=%u s=%u a0=$%06" PRIX32 " d4=$%08" PRIX32 " a7=$%06" PRIX32 " ssp=$%06" PRIX32 " usp=$%06" PRIX32 " mode=%ux%u bpp=%u scan=%d base=$%06" PRIX32 " res=%u pal0=$%03X mmu:%02X mfp:vr=%02X ipr=%02X/%02X imr=%02X/%02X mem:r=%llu w=%llu be=%llu last_be:%c@$%06" PRIX32 " uniform=%lu acia=st:%02X ctl:%02X tx:%02X rx:%u",
                     (unsigned long)frames_this_sec,
                     cpu_state.pc,
                     (unsigned)opcode,
                     (unsigned)opcode1,
                     (unsigned)opcode2,
                     (unsigned)cpu_state.sr,
                     cpu_state.pending_irq,
                     (unsigned)cpu_state.halted,
                     (unsigned)cpu_state.stopped,
                     cpu_state.a[0],
                     cpu_state.d[4],
                     cpu_state.a[7],
                     cpu_state.ssp,
                     cpu_state.usp,
                     (unsigned)mode.width,
                     (unsigned)mode.height,
                     (unsigned)mode.bpp,
                     st_glue_get_scanline(),
                     video_base,
                     (unsigned)res,
                     (unsigned)(pal0 & 0x0FFF),
                     (unsigned)mmu_cfg,
                     (unsigned)mfp_vr,
                     (unsigned)mfp_ipra,
                     (unsigned)mfp_iprb,
                     (unsigned)mfp_imra,
                     (unsigned)mfp_imrb,
                     (unsigned long long)mem_reads,
                     (unsigned long long)mem_writes,
                     (unsigned long long)mem_bus_errors,
                     last_bus_error_write ? 'W' : 'R',
                     last_bus_error_addr,
                     (unsigned long)uniform_frames,
                     (unsigned)acia_dbg.kbd_status,
                     (unsigned)acia_dbg.kbd_control,
                     (unsigned)acia_dbg.kbd_last_tx,
                     (unsigned)acia_dbg.kbd_rx_pending);

            frames_this_sec = 0;
            last_dbg_us = now_us;
        }

        vTaskDelay(pdMS_TO_TICKS(1));

        vTaskDelayUntil(&last_wake, frame_ticks);
    }

    s_emu_task = NULL;
    if (s_state != ESPTARI_STATE_ERROR) {
        s_state = ESPTARI_STATE_STOPPED;
    }
    vTaskDelete(NULL);
}

esp_err_t esptari_core_init(void)
{
    s_machine = ESPTARI_MACHINE_ST;
    s_core_ready = false;
    s_stop_requested = false;
    s_loop_stall_samples = 0;
    s_loop_trace_captured = false;
    s_system = NULL;
    s_emu_task = NULL;
    s_state = ESPTARI_STATE_STOPPED;
    ESP_LOGI(TAG, "Core emulation framework initialized");
    return ESP_OK;
}

esp_err_t esptari_core_load_machine(esptari_machine_t machine)
{
    ESP_LOGI(TAG, "Loading machine profile %d", (int)machine);
    s_machine = machine;
    return ESP_OK;
}

esp_err_t esptari_core_start(void)
{
    if (s_state == ESPTARI_STATE_RUNNING) {
        return ESP_OK;
    }

    esp_err_t ret = setup_static_components();
    if (ret != ESP_OK) {
        s_state = ESPTARI_STATE_ERROR;
        return ret;
    }

    if (s_state == ESPTARI_STATE_PAUSED) {
        s_state = ESPTARI_STATE_RUNNING;
        ESP_LOGI(TAG, "Emulation resumed");
        return ESP_OK;
    }

    s_stop_requested = false;
    s_loop_stall_samples = 0;
    s_loop_trace_captured = false;
    if (!s_emu_task) {
        BaseType_t ok = xTaskCreatePinnedToCore(
            emulation_task,
            "esptari_emu",
            12288,
            NULL,
            4,
            &s_emu_task,
            0
        );
        if (ok != pdPASS) {
            ESP_LOGE(TAG, "Failed to create emulation task");
            s_state = ESPTARI_STATE_ERROR;
            return ESP_FAIL;
        }
    }

    s_state = ESPTARI_STATE_RUNNING;
    ESP_LOGI(TAG, "Starting emulation");
    return ESP_OK;
}

void esptari_core_pause(void)
{
    if (s_state == ESPTARI_STATE_RUNNING) {
        s_state = ESPTARI_STATE_PAUSED;
        ESP_LOGI(TAG, "Emulation paused");
    }
}

void esptari_core_resume(void)
{
    if (s_state == ESPTARI_STATE_PAUSED) {
        s_state = ESPTARI_STATE_RUNNING;
        ESP_LOGI(TAG, "Emulation resumed");
    }
}

void esptari_core_stop(void)
{
    s_stop_requested = true;

    for (int i = 0; i < 50 && s_emu_task != NULL; i++) {
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    if (s_emu_task != NULL) {
        vTaskDelete(s_emu_task);
        s_emu_task = NULL;
    }

    s_state = ESPTARI_STATE_STOPPED;
    ESP_LOGI(TAG, "Emulation stopped");
}

esptari_state_t esptari_core_get_state(void)
{
    return s_state;
}

void esptari_core_reset(void)
{
    st_memory_reset();
    setup_reset_vectors();
    st_glue_reset();
    s_loop_stall_samples = 0;
    s_loop_trace_captured = false;
    if (s_system && s_system->reset) {
        s_system->reset();
    } else {
        if (s_mfp && s_mfp->reset) s_mfp->reset();
        if (s_video && s_video->reset) s_video->reset();
        if (s_audio && s_audio->reset) s_audio->reset();
        if (s_cpu && s_cpu->reset) s_cpu->reset();
    }
    ESP_LOGI(TAG, "Machine reset");
}

esp_err_t esptari_core_dump_stacktrace(const char *path, uint32_t stack_words)
{
    if (!path || path[0] == '\0') {
        return ESP_ERR_INVALID_ARG;
    }

    if (!s_cpu || !s_cpu->get_state) {
        return ESP_ERR_INVALID_STATE;
    }

    if (stack_words == 0U) {
        stack_words = 64U;
    }
    if (stack_words > 512U) {
        stack_words = 512U;
    }

    (void)mkdir("/sdcard/logs", 0755);

    FILE *f = fopen(path, "w");
    if (!f) {
        ESP_LOGE(TAG, "Failed to open stacktrace file: %s", path);
        return ESP_FAIL;
    }

    cpu_state_t cpu_state;
    memset(&cpu_state, 0, sizeof(cpu_state));
    s_cpu->get_state(&cpu_state);

    bus_interface_t *bus = st_memory_get_bus();
    uint16_t opcode0 = 0;
    uint16_t opcode1 = 0;
    uint16_t opcode2 = 0;

    if (bus && bus->read_word) {
        opcode0 = bus->read_word(cpu_state.pc & 0x00FFFFFEU);
        opcode1 = bus->read_word((cpu_state.pc + 2U) & 0x00FFFFFEU);
        opcode2 = bus->read_word((cpu_state.pc + 4U) & 0x00FFFFFEU);
    }

    uint32_t active_sp = cpu_state.a[7] & 0x00FFFFFFU;

    fprintf(f, "espTari stacktrace\n");
    fprintf(f, "timestamp_ms=%lld\n", (long long)esp_log_timestamp());
    fprintf(f, "state=%d machine=%d\n", (int)s_state, (int)s_machine);
    fprintf(f, "pc=%06" PRIX32 " sr=%04X irq=%d halted=%u stopped=%u\n",
            cpu_state.pc & 0x00FFFFFFU,
            (unsigned)cpu_state.sr,
            cpu_state.pending_irq,
            (unsigned)cpu_state.halted,
            (unsigned)cpu_state.stopped);
    fprintf(f, "op=%04X/%04X/%04X\n", (unsigned)opcode0, (unsigned)opcode1, (unsigned)opcode2);
    fprintf(f, "ssp=%06" PRIX32 " usp=%06" PRIX32 " a7=%06" PRIX32 " active_sp=%06" PRIX32 "\n",
            cpu_state.ssp & 0x00FFFFFFU,
            cpu_state.usp & 0x00FFFFFFU,
            cpu_state.a[7] & 0x00FFFFFFU,
            active_sp);

    fprintf(f, "dregs:");
    for (int i = 0; i < 8; i++) {
        fprintf(f, " D%d=%08" PRIX32, i, cpu_state.d[i]);
    }
    fprintf(f, "\n");

    fprintf(f, "aregs:");
    for (int i = 0; i < 8; i++) {
        fprintf(f, " A%d=%06" PRIX32, i, cpu_state.a[i] & 0x00FFFFFFU);
    }
    fprintf(f, "\n");

    fprintf(f, "stack_words=%u\n", (unsigned)stack_words);
    {
        char microtrace_buf[4096];
        int microtrace_len = m68k_get_microtrace_text(microtrace_buf, (int)sizeof(microtrace_buf));
        fprintf(f, "microtrace_bytes=%d\n", microtrace_len);
        if (microtrace_len > 0) {
            fprintf(f, "microtrace:\n%s", microtrace_buf);
        }
    }
    fprintf(f, "pc_window_words:\n");
    if (bus && bus->read_word) {
        for (int i = -8; i <= 8; i++) {
            uint32_t addr = (uint32_t)(((int32_t)cpu_state.pc + (i * 2)) & 0x00FFFFFE);
            uint16_t word = bus->read_word(addr);
            fprintf(f, "pc%+03d: %06" PRIX32 " = %04X\n", i * 2, addr, (unsigned)word);
        }
    }

    if (bus && bus->read_word) {
        for (uint32_t i = 0; i < stack_words; i++) {
            uint32_t addr = (active_sp + (i * 2U)) & 0x00FFFFFEU;
            uint16_t word = bus->read_word(addr);
            fprintf(f, "%02u: %06" PRIX32 " = %04X\n", (unsigned)i, addr, (unsigned)word);
        }
    } else {
        fprintf(f, "bus_unavailable\n");
    }

    fclose(f);
    ESP_LOGI(TAG, "Stacktrace written: %s (%u words)", path, (unsigned)stack_words);
    return ESP_OK;
}
