/**
 * @file st_hw_test_main.c
 * @brief Atari ST Hardware Integration Test
 * 
 * Loads all Phase 2 EBIN components (CPU, MFP, Shifter, YM2149),
 * initializes the ST memory map, loads TOS ROM from SD card, and
 * attempts to boot TOS.
 * 
 * Test strategy:
 *   1. Mount SD card
 *   2. Initialize ST memory (4MB RAM)
 *   3. Load TOS ROM
 *   4. Load EBIN components (m68000, mfp68901, shifter, ym2149)
 *   5. Wire components together (bus, I/O handlers, interrupts)
 *   6. Run CPU for N frames and observe progress
 *   7. Check for signs of TOS boot (vector table setup, etc.)
 * 
 * SPDX-License-Identifier: MIT
 */

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include "driver/sdmmc_host.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"

#include "esptari_loader.h"
#include "component_api.h"
#include "esptari_memory.h"
#include "st_glue.h"
#include "st_acia.h"
#include "sd_pwr_ctrl_by_on_chip_ldo.h"

static const char *TAG = "st_hw_test";

/*===========================================================================*/
/* Configuration                                                             */
/*===========================================================================*/

#define MOUNT_POINT     "/sdcard"

/* EBIN paths on SD card */
#define CPU_EBIN_PATH   "/sdcard/cores/cpu/m68000.ebin"
#define MFP_EBIN_PATH   "/sdcard/cores/misc/mfp68901.ebin"
#define VIDEO_EBIN_PATH "/sdcard/cores/video/shifter.ebin"
#define AUDIO_EBIN_PATH "/sdcard/cores/audio/ym2149.ebin"

/* TOS ROM path */
#define TOS_ROM_PATH     "/sdcard/roms/tos/tos104us.img"

/* ST configuration */
#define ST_RAM_SIZE     (4 * 1024 * 1024)   /* 4MB RAM */

/* SD card pins for ESP32-P4-NANO */
#define SD_CMD_PIN  44
#define SD_CLK_PIN  43
#define SD_D0_PIN   39
#define SD_D1_PIN   40
#define SD_D2_PIN   41
#define SD_D3_PIN   42

/*===========================================================================*/
/* SD Card Mount                                                             */
/*===========================================================================*/

static sdmmc_card_t *s_card = NULL;

static esp_err_t mount_sdcard(void)
{
    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = false,
        .max_files = 10,
        .allocation_unit_size = 16 * 1024
    };

    sdmmc_host_t host = SDMMC_HOST_DEFAULT();

    sd_pwr_ctrl_ldo_config_t ldo_config = { .ldo_chan_id = 4 };
    sd_pwr_ctrl_handle_t pwr_ctrl_handle = NULL;
    esp_err_t ret = sd_pwr_ctrl_new_on_chip_ldo(&ldo_config, &pwr_ctrl_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "LDO power init failed: %s", esp_err_to_name(ret));
        return ret;
    }
    host.pwr_ctrl_handle = pwr_ctrl_handle;

    sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();
    slot_config.width = 4;
    slot_config.clk = SD_CLK_PIN;
    slot_config.cmd = SD_CMD_PIN;
    slot_config.d0  = SD_D0_PIN;
    slot_config.d1  = SD_D1_PIN;
    slot_config.d2  = SD_D2_PIN;
    slot_config.d3  = SD_D3_PIN;
    slot_config.flags |= SDMMC_SLOT_FLAG_INTERNAL_PULLUP;

    ret = esp_vfs_fat_sdmmc_mount(MOUNT_POINT, &host, &slot_config,
                                  &mount_config, &s_card);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "SD mount failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "SD card mounted successfully");
    return ESP_OK;
}

static void unmount_sdcard(void)
{
    if (s_card) {
        esp_vfs_fat_sdcard_unmount(MOUNT_POINT, s_card);
        s_card = NULL;
    }
}

/*===========================================================================*/
/* Component State                                                           */
/*===========================================================================*/

static cpu_interface_t   *s_cpu   = NULL;
static io_interface_t    *s_mfp   = NULL;
static video_interface_t *s_video = NULL;
static audio_interface_t *s_audio = NULL;

/*===========================================================================*/
/* I/O Bridge Functions                                                      */
/* Connect EBIN I/O interfaces to the memory map's io_handler_t              */
/*===========================================================================*/

/* MFP I/O bridge */
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

/* Video I/O bridge (Shifter register reads/writes) */
static uint8_t video_io_bridge_read_byte(uint32_t addr, void *ctx)
{
    video_interface_t *v = (video_interface_t *)ctx;
    /* Read word, extract the correct byte */
    uint16_t w = v->read_reg(addr & ~1);
    return (addr & 1) ? (uint8_t)(w & 0xFF) : (uint8_t)(w >> 8);
}

static uint16_t video_io_bridge_read_word(uint32_t addr, void *ctx)
{
    video_interface_t *v = (video_interface_t *)ctx;
    return v->read_reg(addr);
}

static void video_io_bridge_write_byte(uint32_t addr, uint8_t val, void *ctx)
{
    video_interface_t *v = (video_interface_t *)ctx;
    /* Byte writes to video registers - write as word with byte in position */
    v->write_reg(addr, (uint16_t)val);
}

static void video_io_bridge_write_word(uint32_t addr, uint16_t val, void *ctx)
{
    video_interface_t *v = (video_interface_t *)ctx;
    v->write_reg(addr, val);
}

/* Audio I/O bridge (YM2149 register access) */
static uint8_t audio_io_bridge_read_byte(uint32_t addr, void *ctx)
{
    audio_interface_t *a = (audio_interface_t *)ctx;
    return a->read_reg(addr);
}

static uint16_t audio_io_bridge_read_word(uint32_t addr, void *ctx)
{
    audio_interface_t *a = (audio_interface_t *)ctx;
    /* PSG returns data in low byte */
    return 0xFF00 | a->read_reg(addr);
}

static void audio_io_bridge_write_byte(uint32_t addr, uint8_t val, void *ctx)
{
    audio_interface_t *a = (audio_interface_t *)ctx;
    a->write_reg(addr, val);
}

static void audio_io_bridge_write_word(uint32_t addr, uint16_t val, void *ctx)
{
    audio_interface_t *a = (audio_interface_t *)ctx;
    /* PSG uses low byte for data */
    a->write_reg(addr, (uint8_t)(val & 0xFF));
}

/*===========================================================================*/
/* GLUE MFP Bridge                                                           */
/* Connects GLUE's timing to MFP's clock and IRQ functions                   */
/*===========================================================================*/

static void glue_mfp_clock_bridge(int cycles)
{
    if (s_mfp && s_mfp->clock) {
        s_mfp->clock(cycles);
    }
}

static bool glue_mfp_irq_bridge(void)
{
    if (s_mfp && s_mfp->irq_pending) {
        return s_mfp->irq_pending();
    }
    return false;
}

/*===========================================================================*/
/* Boot ROM Setup                                                            */
/*===========================================================================*/

/**
 * @brief Set up the 68000 reset vectors in RAM from ROM
 * 
 * On a real ST, the GLUE maps ROM to address 0 for the first
 * two long words read after reset (initial SSP and PC), then
 * switches to RAM. We'll copy the reset vectors from ROM to RAM
 * before resetting the CPU.
 */
static void setup_reset_vectors(void)
{
    uint8_t *ram = st_memory_get_ram();
    uint8_t *rom = st_memory_get_rom();
    uint32_t rom_size = st_memory_get_rom_size();
    
    if (!ram || !rom || rom_size < 8) {
        ESP_LOGE(TAG, "Cannot setup reset vectors: RAM or ROM not available");
        return;
    }
    
    /* Copy first 8 bytes (SSP + PC) from ROM to RAM address 0 */
    memcpy(ram, rom, 8);
    
    uint32_t initial_ssp = ((uint32_t)ram[0] << 24) | ((uint32_t)ram[1] << 16) |
                           ((uint32_t)ram[2] << 8) | ram[3];
    uint32_t initial_pc  = ((uint32_t)ram[4] << 24) | ((uint32_t)ram[5] << 16) |
                           ((uint32_t)ram[6] << 8) | ram[7];
    
    ESP_LOGI(TAG, "Reset vectors: SSP=$%08lX PC=$%08lX",
             (unsigned long)initial_ssp, (unsigned long)initial_pc);
}

/*===========================================================================*/
/* Test: Basic Memory Map                                                    */
/*===========================================================================*/

static void test_memory_map(void)
{
    ESP_LOGI(TAG, "=== Test: Memory Map ===");
    
    bus_interface_t *bus = st_memory_get_bus();
    
    /* Test RAM write/read */
    bus->write_long(0x1000, 0xDEADBEEF);
    uint32_t val = bus->read_long(0x1000);
    ESP_LOGI(TAG, "  RAM write/read: $%08lX %s", 
             (unsigned long)val,
             val == 0xDEADBEEF ? "PASS" : "FAIL");
    
    /* Test ROM read (should return ROM data) */
    uint8_t *rom = st_memory_get_rom();
    if (rom && st_memory_get_rom_size() > 0) {
        uint8_t rom_byte = bus->read_byte(0xFC0000);
        ESP_LOGI(TAG, "  ROM read $FC0000: $%02X (expected $%02X) %s",
                 rom_byte, rom[0],
                 rom_byte == rom[0] ? "PASS" : "FAIL");
    } else {
        ESP_LOGW(TAG, "  ROM not loaded, skipping ROM test");
    }
    
    /* Test I/O space read (unhandled returns $FF) */
    uint8_t io_val = bus->read_byte(0xFF9000);
    ESP_LOGI(TAG, "  Unhandled I/O read: $%02X %s",
             io_val, io_val == 0xFF ? "PASS" : "FAIL");
}

/*===========================================================================*/
/* Test: MFP Timer C (200Hz heartbeat)                                       */
/*===========================================================================*/

static void test_mfp_timer_c(void)
{
    ESP_LOGI(TAG, "=== Test: MFP Timer C ===");
    
    if (!s_mfp) {
        ESP_LOGW(TAG, "  MFP not loaded, skipping");
        return;
    }
    
    /* Configure Timer C for 200Hz:
     * MFP clock = 2.4576 MHz
     * Timer C: prescaler /64, data = 192
     * Frequency = 2457600 / (64 * 192) = 200 Hz
     */
    
    /* Reset MFP */
    s_mfp->reset();
    
    /* Write Timer C data register ($FFFA23) = 192 */
    s_mfp->write_byte(0xFFFA23, 192);
    
    /* Enable Timer C interrupt */
    s_mfp->write_byte(0xFFFA09, 0x20);  /* IERB bit 5 */
    s_mfp->write_byte(0xFFFA15, 0x20);  /* IMRB bit 5 */
    
    /* Start Timer C with /64 prescaler */
    /* TCDCR ($FFFA1D): Timer C bits 6-4 = 5 (/64), Timer D bits 2-0 = 0 */
    s_mfp->write_byte(0xFFFA1D, 0x50);
    
    /* Clock the MFP for enough cycles to get a Timer C interrupt
     * 64 * 192 = 12288 MFP cycles for one Timer C period */
    int tick_count = 0;
    for (int cycle = 0; cycle < 15000; cycle++) {
        s_mfp->clock(1);
        if (s_mfp->irq_pending()) {
            tick_count++;
            /* Acknowledge by reading vector */
            uint8_t vec = s_mfp->get_vector();
            ESP_LOGD(TAG, "  MFP IRQ vector: $%02X", vec);
        }
    }
    
    ESP_LOGI(TAG, "  Timer C ticks in 15000 MFP cycles: %d %s",
             tick_count, tick_count >= 1 ? "PASS" : "FAIL");
}

/*===========================================================================*/
/* Test: TOS Boot (run CPU with full ST hardware)                            */
/*===========================================================================*/

static void test_tos_boot(void)
{
    ESP_LOGI(TAG, "=== Test: TOS Boot Attempt ===");
    
    if (!s_cpu) {
        ESP_LOGE(TAG, "  CPU not loaded, cannot boot");
        return;
    }
    
    if (!st_memory_get_rom() || st_memory_get_rom_size() == 0) {
        ESP_LOGW(TAG, "  No TOS ROM loaded, skipping boot test");
        return;
    }
    
    /* Get bus interface */
    bus_interface_t *bus = st_memory_get_bus();
    
    /* Set up reset vectors from ROM */
    setup_reset_vectors();
    
    /* Connect GLUE to CPU's set_irq */
    st_glue_connect_cpu(s_cpu->set_irq);
    
    /* Connect GLUE to MFP */
    if (s_mfp) {
        st_glue_connect_mfp(glue_mfp_clock_bridge, glue_mfp_irq_bridge);
        
        /* Initialize MFP (io_config_t has only context pointer) */
        io_config_t mfp_config = { .context = NULL };
        s_mfp->init((io_config_t *)&mfp_config);
    }
    
    /* Initialize CPU (must be before set_bus, since init resets bus to null) */
    cpu_config_t cpu_cfg = {
        .clock_hz = ST_CPU_CLOCK_HZ,
        .context = NULL,
    };
    s_cpu->init((cpu_config_t *)&cpu_cfg);
    
    /* Connect CPU to bus AFTER init (init resets bus handlers to null stubs) */
    s_cpu->set_bus(bus);
    
    /* Reset everything */
    st_memory_reset();
    setup_reset_vectors();  /* Re-setup after RAM clear */
    
    if (s_mfp) s_mfp->reset();
    if (s_video) s_video->reset();
    if (s_audio) s_audio->reset();
    st_glue_reset();
    s_cpu->reset();
    
    ESP_LOGI(TAG, "  Starting TOS boot...");
    
    /* Trace first 20 instructions to debug boot */
    {
        cpu_state_t ts;
        s_cpu->get_state(&ts);
        ESP_LOGI(TAG, "  After reset: PC=$%06lX SSP=$%08lX",
                 (unsigned long)ts.pc, (unsigned long)ts.a[7]);
        
        bus_interface_t *tbus = st_memory_get_bus();
        for (int i = 0; i < 20; i++) {
            s_cpu->get_state(&ts);
            uint16_t op = tbus->read_word(ts.pc);
            ESP_LOGI(TAG, "  [%02d] PC=$%06lX op=$%04X SR=$%04X A7=$%08lX",
                     i, (unsigned long)ts.pc, op, ts.sr,
                     (unsigned long)ts.a[7]);
            int cyc = s_cpu->execute(1);
            if (ts.halted) {
                ESP_LOGW(TAG, "  CPU halted at step %d", i);
                break;
            }
            (void)cyc;
        }
        s_cpu->get_state(&ts);
        ESP_LOGI(TAG, "  After trace: PC=$%06lX A7=$%08lX",
                 (unsigned long)ts.pc, (unsigned long)ts.a[7]);
    }
    
    /* Run for several frames worth of cycles */
    int total_cycles = 0;
    int target_cycles = ST_CYCLES_PER_FRAME * 5;  /* 5 frames */
    int chunk = 100;  /* Execute in chunks for GLUE timing */
    
    int64_t start_time = esp_timer_get_time();
    
    while (total_cycles < target_cycles) {
        int executed = s_cpu->execute(chunk);
        total_cycles += executed;
        
        /* Clock GLUE (which clocks MFP and generates interrupts) */
        st_glue_clock(executed);
    }
    
    int64_t end_time = esp_timer_get_time();
    int64_t elapsed_us = end_time - start_time;
    
    /* Read CPU state */
    cpu_state_t state;
    s_cpu->get_state(&state);
    
    ESP_LOGI(TAG, "  Executed %d cycles in %lld us (%.1f MHz effective)",
             total_cycles, elapsed_us,
             (double)total_cycles / (double)elapsed_us);
    
    ESP_LOGI(TAG, "  CPU State after boot attempt:");
    ESP_LOGI(TAG, "    PC=$%08lX  SR=$%04X",
             (unsigned long)state.pc, state.sr);
    ESP_LOGI(TAG, "    D0=$%08lX D1=$%08lX D2=$%08lX D3=$%08lX",
             (unsigned long)state.d[0], (unsigned long)state.d[1],
             (unsigned long)state.d[2], (unsigned long)state.d[3]);
    ESP_LOGI(TAG, "    A0=$%08lX A7=$%08lX",
             (unsigned long)state.a[0], (unsigned long)state.a[7]);
    ESP_LOGI(TAG, "    Frames: %lu  Scanline: %d",
             (unsigned long)st_glue_get_frame_count(),
             st_glue_get_scanline());
    
    /* Check for signs of TOS progress */
    uint8_t *ram = st_memory_get_ram();
    
    /* Check memtop ($42E) - TOS writes detected RAM size here
     * During boot, TOS probes RAM and writes the top address */
    uint32_t memtop = ((uint32_t)ram[0x42E] << 24) |
                      ((uint32_t)ram[0x42F] << 16) |
                      ((uint32_t)ram[0x430] << 8) |
                      ram[0x431];
    
    /* Check phystop ($43E) - physical top of RAM */
    uint32_t phystop = ((uint32_t)ram[0x43E] << 24) |
                       ((uint32_t)ram[0x43F] << 16) |
                       ((uint32_t)ram[0x440] << 8) |
                       ram[0x441];
    
    /* Check _bootdev ($446) */
    uint16_t bootdev = ((uint16_t)ram[0x446] << 8) | ram[0x447];
    
    ESP_LOGI(TAG, "  System variables:");
    ESP_LOGI(TAG, "    memtop  ($42E): $%08lX", (unsigned long)memtop);
    ESP_LOGI(TAG, "    phystop ($43E): $%08lX", (unsigned long)phystop);
    ESP_LOGI(TAG, "    bootdev ($446): $%04X", bootdev);
    
    /* Check if PC is in ROM space - indicates TOS is running */
    bool pc_in_rom = (state.pc >= 0xFC0000 && state.pc < 0xFF0000) ||
                     (state.pc >= 0xE00000 && state.pc < 0xF00000);
    
    ESP_LOGI(TAG, "  PC in ROM: %s", pc_in_rom ? "YES" : "NO");
    
    if (pc_in_rom && !state.halted) {
        ESP_LOGI(TAG, "  TOS appears to be executing (PASS)");
    } else if (state.halted) {
        ESP_LOGW(TAG, "  CPU halted (may need more hardware stubs)");
    } else {
        ESP_LOGW(TAG, "  PC not in ROM - TOS may have crashed or not started");
    }
}

/*===========================================================================*/
/* Main                                                                      */
/*===========================================================================*/

void app_main(void)
{
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "  Atari ST Hardware Integration Test");
    ESP_LOGI(TAG, "  Phase 2: Memory + GLUE + Chips");
    ESP_LOGI(TAG, "========================================");
    
    /* Mount SD card */
    ESP_LOGI(TAG, "Mounting SD card...");
    esp_err_t ret = mount_sdcard();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "FATAL: Cannot mount SD card");
        return;
    }
    
    /* Initialize ST memory subsystem */
    ESP_LOGI(TAG, "Initializing ST memory (%dMB RAM)...", ST_RAM_SIZE / (1024*1024));
    ret = st_memory_init(ST_RAM_SIZE);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "FATAL: Memory init failed: %s", esp_err_to_name(ret));
        goto cleanup;
    }
    
    /* Load TOS ROM */
    ESP_LOGI(TAG, "Loading TOS ROM...");
    ret = st_memory_load_rom(TOS_ROM_PATH);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "TOS ROM not found at %s (non-fatal, some tests skipped)",
                 TOS_ROM_PATH);
    }
    
    /* Initialize GLUE chip */
    ESP_LOGI(TAG, "Initializing GLUE (PAL 50Hz)...");
    st_glue_init(true);  /* PAL */
    
    /* Initialize ACIA stubs */
    ESP_LOGI(TAG, "Initializing ACIA stubs...");
    st_acia_init();
    
    /* Load EBIN components */
    ESP_LOGI(TAG, "Loading EBIN components...");
    
    ret = loader_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "EBIN loader init failed: %s", esp_err_to_name(ret));
        goto cleanup;
    }
    
    /* CPU */
    void *cpu_iface = NULL;
    ret = loader_load_component(CPU_EBIN_PATH, COMPONENT_TYPE_CPU, &cpu_iface);
    if (ret == ESP_OK && cpu_iface) {
        s_cpu = (cpu_interface_t *)cpu_iface;
        ESP_LOGI(TAG, "  CPU: %s (v%08lX)", s_cpu->name,
                 (unsigned long)s_cpu->interface_version);
    } else {
        ESP_LOGE(TAG, "  CPU load failed: %s", esp_err_to_name(ret));
    }
    
    /* MFP 68901 */
    void *mfp_iface = NULL;
    ret = loader_load_component(MFP_EBIN_PATH, COMPONENT_TYPE_IO, &mfp_iface);
    if (ret == ESP_OK && mfp_iface) {
        s_mfp = (io_interface_t *)mfp_iface;
        ESP_LOGI(TAG, "  MFP: %s (v%08lX)", s_mfp->name,
                 (unsigned long)s_mfp->interface_version);
        
        /* Register MFP as I/O handler */
        io_handler_t mfp_handler = {
            .base       = IO_MFP_BASE,
            .end        = IO_MFP_END,
            .read_byte  = mfp_io_bridge_read_byte,
            .read_word  = mfp_io_bridge_read_word,
            .write_byte = mfp_io_bridge_write_byte,
            .write_word = mfp_io_bridge_write_word,
            .context    = s_mfp,
            .name       = "MFP 68901",
        };
        st_memory_register_io(&mfp_handler);
    } else {
        ESP_LOGW(TAG, "  MFP load failed: %s (non-fatal)", esp_err_to_name(ret));
    }
    
    /* Shifter Video */
    void *video_iface = NULL;
    ret = loader_load_component(VIDEO_EBIN_PATH, COMPONENT_TYPE_VIDEO, &video_iface);
    if (ret == ESP_OK && video_iface) {
        s_video = (video_interface_t *)video_iface;
        ESP_LOGI(TAG, "  Video: %s (v%08lX)", s_video->name,
                 (unsigned long)s_video->interface_version);
        
        /* Register Shifter I/O handlers for registers and palette */
        io_handler_t video_handler = {
            .base       = IO_VIDEO_BASE,
            .end        = IO_VIDEO_END,
            .read_byte  = video_io_bridge_read_byte,
            .read_word  = video_io_bridge_read_word,
            .write_byte = video_io_bridge_write_byte,
            .write_word = video_io_bridge_write_word,
            .context    = s_video,
            .name       = "Shifter",
        };
        st_memory_register_io(&video_handler);
        
        /* Connect Shifter to bus for video RAM DMA */
        bus_interface_t *bus = st_memory_get_bus();
        s_video->set_bus(bus);
    } else {
        ESP_LOGW(TAG, "  Video load failed: %s (non-fatal)", esp_err_to_name(ret));
    }
    
    /* YM2149 PSG */
    void *audio_iface = NULL;
    ret = loader_load_component(AUDIO_EBIN_PATH, COMPONENT_TYPE_AUDIO, &audio_iface);
    if (ret == ESP_OK && audio_iface) {
        s_audio = (audio_interface_t *)audio_iface;
        ESP_LOGI(TAG, "  Audio: %s (v%08lX)", s_audio->name,
                 (unsigned long)s_audio->interface_version);
        
        /* Register YM2149 I/O handler */
        io_handler_t audio_handler = {
            .base       = IO_PSG_BASE,
            .end        = IO_PSG_END,
            .read_byte  = audio_io_bridge_read_byte,
            .read_word  = audio_io_bridge_read_word,
            .write_byte = audio_io_bridge_write_byte,
            .write_word = audio_io_bridge_write_word,
            .context    = s_audio,
            .name       = "YM2149",
        };
        st_memory_register_io(&audio_handler);
    } else {
        ESP_LOGW(TAG, "  Audio load failed: %s (non-fatal)", esp_err_to_name(ret));
    }
    
    /* Run tests */
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "Running hardware tests...");
    ESP_LOGI(TAG, "");
    
    test_memory_map();
    test_mfp_timer_c();
    test_tos_boot();
    
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "  ST Hardware Test Complete");
    ESP_LOGI(TAG, "========================================");
    
cleanup:
    /* Cleanup */
    if (s_cpu) s_cpu->shutdown();
    if (s_mfp) s_mfp->shutdown();
    if (s_video) s_video->shutdown();
    if (s_audio) s_audio->shutdown();
    st_memory_shutdown();
    unmount_sdcard();
}
