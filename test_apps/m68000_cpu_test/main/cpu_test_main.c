/**
 * @file cpu_test_main.c
 * @brief MC68000 CPU EBIN Test Harness
 *
 * Loads m68000.ebin from SD card, provides a bus interface backed by
 * a RAM buffer, loads small 68000 test programs, executes them, and
 * verifies register/flag/memory results.
 *
 * Test programs are written as big-endian 68000 machine code placed
 * into the RAM buffer. The reset vector (address 0) contains the
 * initial SSP and PC values.
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

#include "esptari_loader.h"
#include "component_api.h"
#include "sd_pwr_ctrl_by_on_chip_ldo.h"

static const char *TAG = "m68k_test";

/*===========================================================================*/
/* Configuration                                                             */
/*===========================================================================*/

#define MOUNT_POINT     "/sdcard"
#define EBIN_PATH       "/sdcard/cores/cpu/m68000.ebin"

/* SD card pins for ESP32-P4-NANO */
#define SD_CMD_PIN  44
#define SD_CLK_PIN  43
#define SD_D0_PIN   39
#define SD_D1_PIN   40
#define SD_D2_PIN   41
#define SD_D3_PIN   42

/*===========================================================================*/
/* Test RAM (68000 address space)                                            */
/*===========================================================================*/

/* 64KB should be plenty for instruction tests */
#define RAM_SIZE    (64 * 1024)

static uint8_t s_ram[RAM_SIZE];

/* Write a big-endian word to RAM */
static void ram_write16(uint32_t addr, uint16_t val)
{
    if (addr + 1 < RAM_SIZE) {
        s_ram[addr]     = (val >> 8) & 0xFF;
        s_ram[addr + 1] = val & 0xFF;
    }
}

/* Write a big-endian long to RAM */
static void ram_write32(uint32_t addr, uint32_t val)
{
    if (addr + 3 < RAM_SIZE) {
        s_ram[addr]     = (val >> 24) & 0xFF;
        s_ram[addr + 1] = (val >> 16) & 0xFF;
        s_ram[addr + 2] = (val >> 8) & 0xFF;
        s_ram[addr + 3] = val & 0xFF;
    }
}

/* Read a big-endian word from RAM */
static uint16_t ram_read16(uint32_t addr)
{
    if (addr + 1 < RAM_SIZE) {
        return ((uint16_t)s_ram[addr] << 8) | s_ram[addr + 1];
    }
    return 0xFFFF;
}

/*===========================================================================*/
/* Bus Interface (connects CPU to test RAM)                                  */
/*===========================================================================*/

static uint8_t bus_read_byte(uint32_t addr)
{
    if (addr < RAM_SIZE) return s_ram[addr];
    return 0xFF;
}

static uint16_t bus_read_word(uint32_t addr)
{
    if (addr + 1 < RAM_SIZE) {
        return ((uint16_t)s_ram[addr] << 8) | s_ram[addr + 1];
    }
    return 0xFFFF;
}

static uint32_t bus_read_long(uint32_t addr)
{
    if (addr + 3 < RAM_SIZE) {
        return ((uint32_t)s_ram[addr] << 24) |
               ((uint32_t)s_ram[addr + 1] << 16) |
               ((uint32_t)s_ram[addr + 2] << 8) |
               s_ram[addr + 3];
    }
    return 0xFFFFFFFF;
}

static void bus_write_byte(uint32_t addr, uint8_t val)
{
    if (addr < RAM_SIZE) s_ram[addr] = val;
}

static void bus_write_word(uint32_t addr, uint16_t val)
{
    if (addr + 1 < RAM_SIZE) {
        s_ram[addr]     = (val >> 8) & 0xFF;
        s_ram[addr + 1] = val & 0xFF;
    }
}

static void bus_write_long(uint32_t addr, uint32_t val)
{
    if (addr + 3 < RAM_SIZE) {
        s_ram[addr]     = (val >> 24) & 0xFF;
        s_ram[addr + 1] = (val >> 16) & 0xFF;
        s_ram[addr + 2] = (val >> 8) & 0xFF;
        s_ram[addr + 3] = val & 0xFF;
    }
}

static void bus_error(uint32_t addr, bool write)
{
    ESP_LOGE(TAG, "BUS ERROR at 0x%08lX (%s)", (unsigned long)addr,
             write ? "write" : "read");
}

static void bus_address_error(uint32_t addr, bool write)
{
    ESP_LOGE(TAG, "ADDRESS ERROR at 0x%08lX (%s)", (unsigned long)addr,
             write ? "write" : "read");
}

static bus_interface_t s_bus = {
    .read_byte     = bus_read_byte,
    .read_word     = bus_read_word,
    .read_long     = bus_read_long,
    .write_byte    = bus_write_byte,
    .write_word    = bus_write_word,
    .write_long    = bus_write_long,
    .bus_error     = bus_error,
    .address_error = bus_address_error,
    .context       = NULL,
};

/*===========================================================================*/
/* SD Card Mount/Unmount                                                     */
/*===========================================================================*/

static sdmmc_card_t *s_card = NULL;

static esp_err_t mount_sdcard(void)
{
    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = false,
        .max_files = 5,
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

    ESP_LOGI(TAG, "SD card mounted");
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
/* Test Framework                                                            */
/*===========================================================================*/

static int s_tests_run = 0;
static int s_tests_pass = 0;
static int s_tests_fail = 0;

#define TEST_ASSERT(cond, fmt, ...) do { \
    if (!(cond)) { \
        ESP_LOGE(TAG, "  FAIL: " fmt, ##__VA_ARGS__); \
        s_tests_fail++; \
        return; \
    } \
} while(0)

#define TEST_START(name) \
    ESP_LOGI(TAG, "--- Test: %s ---", name); \
    s_tests_run++

#define TEST_PASS() \
    ESP_LOGI(TAG, "  PASS"); \
    s_tests_pass++

/*===========================================================================*/
/* Helper: Set up reset vectors and a test program                           */
/*===========================================================================*/

/**
 * @brief Clear RAM and set reset vectors.
 *
 * 68000 reset reads:
 *   address 0: Initial SSP
 *   address 4: Initial PC
 *
 * We set SSP = 0x8000 (halfway through our 64K RAM)
 * and PC = 0x0400 (program start)
 */
static void setup_ram(void)
{
    memset(s_ram, 0, RAM_SIZE);
    ram_write32(0x0000, 0x00008000);  /* Initial SSP */
    ram_write32(0x0004, 0x00000400);  /* Initial PC  */
}

/*===========================================================================*/
/* Individual Tests                                                          */
/*===========================================================================*/

static cpu_interface_t *cpu = NULL;

/**
 * Test 1: MOVEQ + NOP + STOP
 *
 * Program at 0x0400:
 *   MOVEQ  #42, D0       ; 7042
 *   MOVEQ  #-1, D1       ; 72FF
 *   NOP                   ; 4E71
 *   STOP   #$2700        ; 4E72 2700  (stop in supervisor mode)
 *
 * Expected: D0=42, D1=0xFFFFFFFF, PC past STOP
 */
static void test_moveq(void)
{
    TEST_START("MOVEQ");

    setup_ram();
    uint32_t pc = 0x0400;
    ram_write16(pc, 0x702A); pc += 2;  /* MOVEQ #42, D0 */
    ram_write16(pc, 0x72FF); pc += 2;  /* MOVEQ #-1, D1 */
    ram_write16(pc, 0x4E71); pc += 2;  /* NOP */
    ram_write16(pc, 0x4E72); pc += 2;  /* STOP */
    ram_write16(pc, 0x2700); pc += 2;  /* #$2700 */

    cpu->set_bus(&s_bus);
    cpu->reset();
    cpu->execute(100);

    cpu_state_t state;
    cpu->get_state(&state);

    TEST_ASSERT(state.d[0] == 42,
                "D0: expected 42, got %lu", (unsigned long)state.d[0]);
    TEST_ASSERT(state.d[1] == 0xFFFFFFFF,
                "D1: expected 0xFFFFFFFF, got 0x%08lX", (unsigned long)state.d[1]);
    TEST_ASSERT(state.stopped == 1,
                "CPU should be stopped");

    TEST_PASS();
}

/**
 * Test 2: MOVE.L and register-to-register
 *
 *   MOVE.L #$12345678, D0     ; 203C 1234 5678
 *   MOVE.L D0, D1             ; 2200
 *   STOP   #$2700             ; 4E72 2700
 */
static void test_move_long(void)
{
    TEST_START("MOVE.L");

    setup_ram();
    uint32_t pc = 0x0400;
    ram_write16(pc, 0x203C); pc += 2;  /* MOVE.L #imm, D0 */
    ram_write16(pc, 0x1234); pc += 2;
    ram_write16(pc, 0x5678); pc += 2;
    ram_write16(pc, 0x2200); pc += 2;  /* MOVE.L D0, D1 */
    ram_write16(pc, 0x4E72); pc += 2;  /* STOP */
    ram_write16(pc, 0x2700); pc += 2;

    cpu->set_bus(&s_bus);
    cpu->reset();
    cpu->execute(200);

    cpu_state_t state;
    cpu->get_state(&state);

    TEST_ASSERT(state.d[0] == 0x12345678,
                "D0: expected 0x12345678, got 0x%08lX", (unsigned long)state.d[0]);
    TEST_ASSERT(state.d[1] == 0x12345678,
                "D1: expected 0x12345678, got 0x%08lX", (unsigned long)state.d[1]);

    TEST_PASS();
}

/**
 * Test 3: ADD and SUB
 *
 *   MOVEQ  #10, D0     ; 700A
 *   MOVEQ  #20, D1     ; 7214
 *   ADD.L  D0, D1      ; D280   (D1 = D1 + D0 = 30)
 *   MOVEQ  #5, D2      ; 7405
 *   SUB.L  D2, D1      ; 9282   (D1 = D1 - D2 = 25)
 *   STOP   #$2700
 */
static void test_add_sub(void)
{
    TEST_START("ADD/SUB");

    setup_ram();
    uint32_t pc = 0x0400;
    ram_write16(pc, 0x700A); pc += 2;  /* MOVEQ #10, D0 */
    ram_write16(pc, 0x7214); pc += 2;  /* MOVEQ #20, D1 */
    ram_write16(pc, 0xD280); pc += 2;  /* ADD.L D0, D1 */
    ram_write16(pc, 0x7405); pc += 2;  /* MOVEQ #5, D2 */
    ram_write16(pc, 0x9282); pc += 2;  /* SUB.L D2, D1 */
    ram_write16(pc, 0x4E72); pc += 2;
    ram_write16(pc, 0x2700); pc += 2;

    cpu->set_bus(&s_bus);
    cpu->reset();
    cpu->execute(200);

    cpu_state_t state;
    cpu->get_state(&state);

    TEST_ASSERT(state.d[0] == 10,
                "D0: expected 10, got %lu", (unsigned long)state.d[0]);
    TEST_ASSERT(state.d[1] == 25,
                "D1: expected 25, got %lu", (unsigned long)state.d[1]);
    TEST_ASSERT(state.d[2] == 5,
                "D2: expected 5, got %lu", (unsigned long)state.d[2]);

    TEST_PASS();
}

/**
 * Test 4: Branch (BEQ/BNE)
 *
 *   MOVEQ  #0, D0       ; 7000           ; D0 = 0 (sets Z flag)
 *   BEQ    .skip        ; 6704           ; branch +4 if Z set
 *   MOVEQ  #99, D1      ; 7263           ; should be SKIPPED
 *   BRA    .done        ; 6002           ; (not reached)
 * .skip:
 *   MOVEQ  #1, D1       ; 7201           ; D1 = 1
 * .done:
 *   STOP   #$2700
 */
static void test_branch(void)
{
    TEST_START("Bcc (BEQ/BNE)");

    setup_ram();
    uint32_t pc = 0x0400;
    ram_write16(pc, 0x7000); pc += 2;  /* MOVEQ #0, D0 */
    ram_write16(pc, 0x6704); pc += 2;  /* BEQ .skip (+4) */
    ram_write16(pc, 0x7263); pc += 2;  /* MOVEQ #99, D1 (skip this) */
    ram_write16(pc, 0x6002); pc += 2;  /* BRA .done (+2) */
    /* .skip: at 0x0408 */
    ram_write16(pc, 0x7201); pc += 2;  /* MOVEQ #1, D1 */
    /* .done: at 0x040A */
    ram_write16(pc, 0x4E72); pc += 2;
    ram_write16(pc, 0x2700); pc += 2;

    cpu->set_bus(&s_bus);
    cpu->reset();
    cpu->execute(200);

    cpu_state_t state;
    cpu->get_state(&state);

    TEST_ASSERT(state.d[0] == 0,
                "D0: expected 0, got %lu", (unsigned long)state.d[0]);
    TEST_ASSERT(state.d[1] == 1,
                "D1: expected 1 (branch taken), got %lu", (unsigned long)state.d[1]);

    TEST_PASS();
}

/**
 * Test 5: JSR/RTS
 *
 *   LEA    sub, A0       ; 41F9 0000 0410  ; A0 = address of sub
 *   JSR    (A0)          ; 4E90            ; call subroutine
 *   STOP   #$2700        ; 4E72 2700       ; land here after RTS
 *   ; ... 
 * sub:  (at 0x0410)
 *   MOVEQ  #77, D0       ; 704D
 *   RTS                   ; 4E75
 */
static void test_jsr_rts(void)
{
    TEST_START("JSR/RTS");

    setup_ram();
    uint32_t pc = 0x0400;
    ram_write16(pc, 0x41F9); pc += 2;  /* LEA (xxx).L, A0 */
    ram_write16(pc, 0x0000); pc += 2;  /*   high word of 0x00000410 */
    ram_write16(pc, 0x0410); pc += 2;  /*   low word */
    ram_write16(pc, 0x4E90); pc += 2;  /* JSR (A0) */
    /* Return here: 0x040A */
    ram_write16(pc, 0x4E72); pc += 2;  /* STOP */
    ram_write16(pc, 0x2700); pc += 2;

    /* Subroutine at 0x0410 */
    pc = 0x0410;
    ram_write16(pc, 0x704D); pc += 2;  /* MOVEQ #77, D0 */
    ram_write16(pc, 0x4E75); pc += 2;  /* RTS */

    cpu->set_bus(&s_bus);
    cpu->reset();
    cpu->execute(300);

    cpu_state_t state;
    cpu->get_state(&state);

    TEST_ASSERT(state.d[0] == 77,
                "D0: expected 77, got %lu", (unsigned long)state.d[0]);
    TEST_ASSERT(state.stopped == 1,
                "CPU should be stopped after RTS returns to STOP");

    TEST_PASS();
}

/**
 * Test 6: DBcc loop (DBRA)
 *
 *   MOVEQ  #4, D0        ; 7004          ; loop counter = 4 (counts 4,3,2,1,0)
 *   MOVEQ  #0, D1        ; 7200          ; accumulator
 * .loop:
 *   ADDQ.L #1, D1        ; 5281          ; D1++
 *   DBRA   D0, .loop     ; 51C8 FFFC     ; decrement D0, branch if != -1
 *   STOP   #$2700
 *
 * Expected: D1 = 5, D0 = 0xFFFF (low word) 
 */
static void test_dbra_loop(void)
{
    TEST_START("DBcc (DBRA loop)");

    setup_ram();
    uint32_t pc = 0x0400;
    ram_write16(pc, 0x7004); pc += 2;  /* MOVEQ #4, D0 */
    ram_write16(pc, 0x7200); pc += 2;  /* MOVEQ #0, D1 */
    /* .loop at 0x0404 */
    ram_write16(pc, 0x5281); pc += 2;  /* ADDQ.L #1, D1 */
    ram_write16(pc, 0x51C8); pc += 2;  /* DBRA D0, ... */
    ram_write16(pc, 0xFFFC); pc += 2;  /*   displacement back to .loop (-4) */
    ram_write16(pc, 0x4E72); pc += 2;  /* STOP */
    ram_write16(pc, 0x2700); pc += 2;

    cpu->set_bus(&s_bus);
    cpu->reset();
    cpu->execute(500);

    cpu_state_t state;
    cpu->get_state(&state);

    TEST_ASSERT(state.d[1] == 5,
                "D1: expected 5 (loop ran 5 times), got %lu",
                (unsigned long)state.d[1]);
    TEST_ASSERT((state.d[0] & 0xFFFF) == 0xFFFF,
                "D0 low word: expected 0xFFFF, got 0x%04lX",
                (unsigned long)(state.d[0] & 0xFFFF));

    TEST_PASS();
}

/**
 * Test 7: Memory write/read (MOVE to/from memory)
 *
 *   MOVE.L #$DEADBEEF, D0   ; 203C DEAD BEEF
 *   LEA    $1000, A0        ; 41F9 0000 1000
 *   MOVE.L D0, (A0)         ; 2080
 *   CLR.L  D0               ; 4280
 *   MOVE.L (A0), D1         ; 2210
 *   STOP   #$2700
 */
static void test_memory(void)
{
    TEST_START("Memory MOVE.L");

    setup_ram();
    uint32_t pc = 0x0400;
    ram_write16(pc, 0x203C); pc += 2;  /* MOVE.L #imm, D0 */
    ram_write16(pc, 0xDEAD); pc += 2;
    ram_write16(pc, 0xBEEF); pc += 2;
    ram_write16(pc, 0x41F9); pc += 2;  /* LEA (xxx).L, A0 */
    ram_write16(pc, 0x0000); pc += 2;
    ram_write16(pc, 0x1000); pc += 2;
    ram_write16(pc, 0x2080); pc += 2;  /* MOVE.L D0, (A0) */
    ram_write16(pc, 0x4280); pc += 2;  /* CLR.L D0 */
    ram_write16(pc, 0x2210); pc += 2;  /* MOVE.L (A0), D1 */
    ram_write16(pc, 0x4E72); pc += 2;  /* STOP */
    ram_write16(pc, 0x2700); pc += 2;

    cpu->set_bus(&s_bus);
    cpu->reset();
    cpu->execute(400);

    cpu_state_t state;
    cpu->get_state(&state);

    /* Check that memory at 0x1000 contains the value */
    uint32_t mem_val = bus_read_long(0x1000);
    TEST_ASSERT(mem_val == 0xDEADBEEF,
                "Memory @0x1000: expected 0xDEADBEEF, got 0x%08lX",
                (unsigned long)mem_val);
    TEST_ASSERT(state.d[0] == 0,
                "D0: expected 0 (cleared), got 0x%08lX", (unsigned long)state.d[0]);
    TEST_ASSERT(state.d[1] == 0xDEADBEEF,
                "D1: expected 0xDEADBEEF, got 0x%08lX", (unsigned long)state.d[1]);

    TEST_PASS();
}

/**
 * Test 8: Shift/Rotate
 *
 *   MOVEQ  #1, D0       ; 7001
 *   LSL.L  #4, D0       ; E988       ; D0 = 0x10
 *   MOVEQ  #-128, D1    ; 7280       ; D1 = 0xFFFFFF80
 *   ASR.L  #2, D1       ; E481       ; D1 = 0xFFFFFFE0 (arithmetic shift preserves sign)
 *   STOP   #$2700
 */
static void test_shift(void)
{
    TEST_START("LSL/ASR");

    setup_ram();
    uint32_t pc = 0x0400;
    ram_write16(pc, 0x7001); pc += 2;  /* MOVEQ #1, D0 */
    ram_write16(pc, 0xE988); pc += 2;  /* LSL.L #4, D0 */
    ram_write16(pc, 0x7280); pc += 2;  /* MOVEQ #-128, D1 */
    ram_write16(pc, 0xE481); pc += 2;  /* ASR.L #2, D1 */
    ram_write16(pc, 0x4E72); pc += 2;
    ram_write16(pc, 0x2700); pc += 2;

    cpu->set_bus(&s_bus);
    cpu->reset();
    cpu->execute(200);

    cpu_state_t state;
    cpu->get_state(&state);

    TEST_ASSERT(state.d[0] == 0x10,
                "D0: expected 0x10, got 0x%08lX", (unsigned long)state.d[0]);
    TEST_ASSERT(state.d[1] == 0xFFFFFFE0,
                "D1: expected 0xFFFFFFE0 (ASR preserves sign), got 0x%08lX",
                (unsigned long)state.d[1]);

    TEST_PASS();
}

/*===========================================================================*/
/* Main                                                                      */
/*===========================================================================*/

void app_main(void)
{
    ESP_LOGI(TAG, "============================================");
    ESP_LOGI(TAG, "   MC68000 CPU EBIN Test Harness");
    ESP_LOGI(TAG, "============================================");
    ESP_LOGI(TAG, "");

    /* Memory info */
    ESP_LOGI(TAG, "MALLOC_CAP_EXEC free: %zu",
             heap_caps_get_free_size(MALLOC_CAP_EXEC));
    ESP_LOGI(TAG, "MALLOC_CAP_SPIRAM free: %zu",
             heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
    ESP_LOGI(TAG, "");

    /* Mount SD card */
    esp_err_t err = mount_sdcard();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "SD card mount failed - cannot continue");
        return;
    }

    /* Check EBIN exists */
    FILE *f = fopen(EBIN_PATH, "rb");
    if (!f) {
        ESP_LOGE(TAG, "EBIN not found: %s", EBIN_PATH);
        ESP_LOGI(TAG, "Run: tools/prep_sdcard.sh to prepare the SD card");
        unmount_sdcard();
        return;
    }
    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    fclose(f);
    ESP_LOGI(TAG, "Found m68000.ebin: %ld bytes", fsize);

    /* Initialize loader */
    err = loader_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Loader init failed: %s", esp_err_to_name(err));
        unmount_sdcard();
        return;
    }

    /* Load M68000 CPU component */
    ESP_LOGI(TAG, "Loading MC68000 CPU component...");
    cpu = NULL;
    err = loader_load_component(EBIN_PATH, COMPONENT_TYPE_CPU, (void**)&cpu);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to load CPU: %s", esp_err_to_name(err));
        loader_shutdown();
        unmount_sdcard();
        return;
    }

    ESP_LOGI(TAG, "CPU loaded successfully!");
    ESP_LOGI(TAG, "  Name: %s", cpu->name);
    ESP_LOGI(TAG, "  Interface version: 0x%08lX",
             (unsigned long)cpu->interface_version);
    ESP_LOGI(TAG, "  Features: 0x%08lX", (unsigned long)cpu->features);
    ESP_LOGI(TAG, "");

    /* Initialize CPU */
    int init_ret = cpu->init(NULL);
    ESP_LOGI(TAG, "CPU init returned: %d", init_ret);

    /* Wire up bus */
    cpu->set_bus(&s_bus);
    ESP_LOGI(TAG, "Bus interface connected");
    ESP_LOGI(TAG, "");

    /* Run tests */
    ESP_LOGI(TAG, "========== Running Tests ==========");
    ESP_LOGI(TAG, "");

    test_moveq();
    test_move_long();
    test_add_sub();
    test_branch();
    test_jsr_rts();
    test_dbra_loop();
    test_memory();
    test_shift();

    /* Summary */
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "========== Test Summary ==========");
    ESP_LOGI(TAG, "  Run:    %d", s_tests_run);
    ESP_LOGI(TAG, "  Passed: %d", s_tests_pass);
    ESP_LOGI(TAG, "  Failed: %d", s_tests_fail);
    ESP_LOGI(TAG, "  Result: %s", s_tests_fail == 0 ? "ALL PASS" : "FAILURES");
    ESP_LOGI(TAG, "==================================");

    /* Cleanup */
    if (cpu->shutdown) cpu->shutdown();
    loader_unload_component(cpu);
    loader_shutdown();
    unmount_sdcard();
}
