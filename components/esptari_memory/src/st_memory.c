/**
 * @file st_memory.c
 * @brief Atari ST Memory Map and Bus Controller Implementation
 * 
 * Implements the complete 16MB Atari ST address space with bus routing
 * to RAM, ROM, and I/O chip handlers.
 * 
 * Bus timing note:
 *   The 68000 uses big-endian byte order. Words and longs must be
 *   aligned to even addresses. The ST GLUE chip generates bus errors
 *   for writes to ROM and some reserved areas.
 * 
 * SPDX-License-Identifier: MIT
 */

#include <string.h>
#include <stdio.h>
#include <inttypes.h>
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "esptari_memory.h"

static const char *TAG = "st_memory";

/*===========================================================================*/
/* Module State                                                              */
/*===========================================================================*/

static st_memory_t s_mem;
static bus_interface_t s_bus;

/*===========================================================================*/
/* I/O Handler Dispatch                                                      */
/*===========================================================================*/

/**
 * @brief Find I/O handler for the given address
 * 
 * Linear scan is fine - we have <16 handlers max and this is
 * only called for the I/O region ($FF8000-$FFFFFF), which is
 * a small fraction of total accesses.
 */
static inline io_handler_t* find_io_handler(uint32_t addr)
{
    for (int i = 0; i < s_mem.io_handler_count; i++) {
        if (addr >= s_mem.io_handlers[i].base && 
            addr <= s_mem.io_handlers[i].end) {
            return &s_mem.io_handlers[i];
        }
    }
    return NULL;
}

/*===========================================================================*/
/* Bus Read Functions                                                        */
/*===========================================================================*/

static uint8_t st_read_byte(uint32_t addr)
{
    addr &= 0x00FFFFFF;  /* 24-bit address bus */
    
    s_mem.reads++;
    
    /* RAM: $000000 - ram_size */
    if (addr < s_mem.ram_size) {
        return s_mem.ram[addr];
    }
    
    /* TOS ROM: $FC0000 - $FEFFFF (or wherever loaded) */
    if (addr >= s_mem.rom_base && addr < (s_mem.rom_base + s_mem.rom_size)) {
        return s_mem.rom[addr - s_mem.rom_base];
    }
    
    /* I/O Space: $FF8000 - $FFFFFF */
    if (addr >= ST_IO_BASE) {
        io_handler_t *h = find_io_handler(addr);
        if (h && h->read_byte) {
            return h->read_byte(addr, h->context);
        }
        /* Unhandled I/O reads return $FF (open bus) */
        ESP_LOGD(TAG, "Unhandled I/O read byte: $%06" PRIX32, addr);
        return 0xFF;
    }
    
    /* Cartridge ROM: $FA0000 - $FBFFFF */
    if (addr >= 0xFA0000 && addr < 0xFC0000 && s_mem.cartridge) {
        uint32_t off = addr - 0xFA0000;
        if (off < s_mem.cart_size) {
            return s_mem.cartridge[off];
        }
    }
    
    /* Above RAM but below ROM - bus error region */
    s_mem.bus_errors++;
    ESP_LOGD(TAG, "Read from unmapped address: $%06" PRIX32, addr);
    return 0xFF;
}

static uint16_t st_read_word(uint32_t addr)
{
    addr &= 0x00FFFFFF;
    
    s_mem.reads++;
    
    /* RAM fast path (most common) */
    if (addr < s_mem.ram_size) {
        return ((uint16_t)s_mem.ram[addr] << 8) | s_mem.ram[addr + 1];
    }
    
    /* TOS ROM */
    if (addr >= s_mem.rom_base && (addr + 1) < (s_mem.rom_base + s_mem.rom_size)) {
        uint32_t off = addr - s_mem.rom_base;
        return ((uint16_t)s_mem.rom[off] << 8) | s_mem.rom[off + 1];
    }
    
    /* I/O Space */
    if (addr >= ST_IO_BASE) {
        io_handler_t *h = find_io_handler(addr);
        if (h && h->read_word) {
            return h->read_word(addr, h->context);
        }
        /* Fall back to two byte reads if only byte handler */
        if (h && h->read_byte) {
            uint16_t hi = h->read_byte(addr, h->context);
            uint16_t lo = h->read_byte(addr + 1, h->context);
            return (hi << 8) | lo;
        }
        ESP_LOGD(TAG, "Unhandled I/O read word: $%06" PRIX32, addr);
        return 0xFFFF;
    }
    
    /* Cartridge */
    if (addr >= 0xFA0000 && addr < 0xFC0000 && s_mem.cartridge) {
        uint32_t off = addr - 0xFA0000;
        if ((off + 1) < s_mem.cart_size) {
            return ((uint16_t)s_mem.cartridge[off] << 8) | s_mem.cartridge[off + 1];
        }
    }
    
    s_mem.bus_errors++;
    return 0xFFFF;
}

static uint32_t st_read_long(uint32_t addr)
{
    addr &= 0x00FFFFFF;
    
    s_mem.reads++;
    
    /* RAM fast path */
    if ((addr + 3) < s_mem.ram_size) {
        return ((uint32_t)s_mem.ram[addr] << 24) |
               ((uint32_t)s_mem.ram[addr + 1] << 16) |
               ((uint32_t)s_mem.ram[addr + 2] << 8) |
               s_mem.ram[addr + 3];
    }
    
    /* Compose from two word reads for ROM/IO */
    uint32_t hi = st_read_word(addr);
    uint32_t lo = st_read_word(addr + 2);
    return (hi << 16) | lo;
}

/*===========================================================================*/
/* Bus Write Functions                                                       */
/*===========================================================================*/

static void st_write_byte(uint32_t addr, uint8_t val)
{
    addr &= 0x00FFFFFF;
    
    s_mem.writes++;
    
    /* RAM */
    if (addr < s_mem.ram_size) {
        s_mem.ram[addr] = val;
        return;
    }
    
    /* I/O Space */
    if (addr >= ST_IO_BASE) {
        io_handler_t *h = find_io_handler(addr);
        if (h && h->write_byte) {
            h->write_byte(addr, val, h->context);
            return;
        }
        ESP_LOGD(TAG, "Unhandled I/O write byte: $%06" PRIX32 " = $%02X", addr, val);
        return;
    }
    
    /* Writes to ROM generate bus error on real hardware */
    if (addr >= s_mem.rom_base && addr < (s_mem.rom_base + s_mem.rom_size)) {
        s_mem.bus_errors++;
        ESP_LOGD(TAG, "Write to ROM: $%06" PRIX32 " = $%02X", addr, val);
        return;
    }
    
    s_mem.bus_errors++;
    ESP_LOGD(TAG, "Write to unmapped: $%06" PRIX32 " = $%02X", addr, val);
}

static void st_write_word(uint32_t addr, uint16_t val)
{
    addr &= 0x00FFFFFF;
    
    s_mem.writes++;
    
    /* RAM fast path */
    if (addr < s_mem.ram_size) {
        s_mem.ram[addr]     = (uint8_t)(val >> 8);
        s_mem.ram[addr + 1] = (uint8_t)(val & 0xFF);
        return;
    }
    
    /* I/O Space */
    if (addr >= ST_IO_BASE) {
        io_handler_t *h = find_io_handler(addr);
        if (h && h->write_word) {
            h->write_word(addr, val, h->context);
            return;
        }
        /* Fall back to two byte writes */
        if (h && h->write_byte) {
            h->write_byte(addr, (uint8_t)(val >> 8), h->context);
            h->write_byte(addr + 1, (uint8_t)(val & 0xFF), h->context);
            return;
        }
        ESP_LOGD(TAG, "Unhandled I/O write word: $%06" PRIX32 " = $%04X", addr, val);
        return;
    }
    
    if (addr >= s_mem.rom_base && addr < (s_mem.rom_base + s_mem.rom_size)) {
        s_mem.bus_errors++;
        return;
    }
    
    s_mem.bus_errors++;
}

static void st_write_long(uint32_t addr, uint32_t val)
{
    addr &= 0x00FFFFFF;
    
    s_mem.writes++;
    
    /* RAM fast path */
    if ((addr + 3) < s_mem.ram_size) {
        s_mem.ram[addr]     = (uint8_t)(val >> 24);
        s_mem.ram[addr + 1] = (uint8_t)(val >> 16);
        s_mem.ram[addr + 2] = (uint8_t)(val >> 8);
        s_mem.ram[addr + 3] = (uint8_t)(val & 0xFF);
        return;
    }
    
    /* Compose from two word writes for I/O etc */
    st_write_word(addr, (uint16_t)(val >> 16));
    st_write_word(addr + 2, (uint16_t)(val & 0xFFFF));
}

/*===========================================================================*/
/* Bus Error / Address Error                                                 */
/*===========================================================================*/

static void st_bus_error(uint32_t addr, bool write)
{
    s_mem.bus_errors++;
    ESP_LOGW(TAG, "BUS ERROR: $%06" PRIX32 " %s", addr, write ? "write" : "read");
}

static void st_address_error(uint32_t addr, bool write)
{
    s_mem.bus_errors++;
    ESP_LOGW(TAG, "ADDRESS ERROR: $%06" PRIX32 " %s", addr, write ? "write" : "read");
}

/*===========================================================================*/
/* Public API                                                                */
/*===========================================================================*/

esp_err_t st_memory_init(uint32_t ram_size)
{
    /* Validate RAM size */
    if (ram_size == 0 || ram_size > ST_RAM_MAX) {
        ESP_LOGE(TAG, "Invalid RAM size: %lu (max %d)", 
                 (unsigned long)ram_size, ST_RAM_MAX);
        return ESP_ERR_INVALID_ARG;
    }
    
    /* Align to 256KB boundary (ST MMU constraint) */
    ram_size = (ram_size + 0x3FFFF) & ~0x3FFFF;
    
    ESP_LOGI(TAG, "Initializing ST memory: %luKB RAM", 
             (unsigned long)(ram_size / 1024));
    
    memset(&s_mem, 0, sizeof(s_mem));
    
    /* Allocate RAM from PSRAM */
    s_mem.ram = heap_caps_calloc(1, ram_size, MALLOC_CAP_SPIRAM);
    if (!s_mem.ram) {
        ESP_LOGE(TAG, "Failed to allocate %luKB RAM from PSRAM",
                 (unsigned long)(ram_size / 1024));
        return ESP_ERR_NO_MEM;
    }
    s_mem.ram_size = ram_size;
    
    /* ROM defaults */
    s_mem.rom_base = ST_ROM_BASE;
    
    /* Set up bus interface */
    s_bus.read_byte     = st_read_byte;
    s_bus.read_word     = st_read_word;
    s_bus.read_long     = st_read_long;
    s_bus.write_byte    = st_write_byte;
    s_bus.write_word    = st_write_word;
    s_bus.write_long    = st_write_long;
    s_bus.bus_error     = st_bus_error;
    s_bus.address_error = st_address_error;
    s_bus.context       = &s_mem;
    
    ESP_LOGI(TAG, "RAM at %p (%luKB), bus interface ready",
             s_mem.ram, (unsigned long)(ram_size / 1024));
    
    return ESP_OK;
}

void st_memory_shutdown(void)
{
    if (s_mem.ram) {
        heap_caps_free(s_mem.ram);
        s_mem.ram = NULL;
    }
    if (s_mem.rom) {
        heap_caps_free(s_mem.rom);
        s_mem.rom = NULL;
    }
    if (s_mem.cartridge) {
        heap_caps_free(s_mem.cartridge);
        s_mem.cartridge = NULL;
    }
    
    ESP_LOGI(TAG, "Memory shutdown. Stats: reads=%llu writes=%llu bus_errors=%llu",
             s_mem.reads, s_mem.writes, s_mem.bus_errors);
    
    memset(&s_mem, 0, sizeof(s_mem));
}

esp_err_t st_memory_load_rom(const char *path)
{
    if (!path) return ESP_ERR_INVALID_ARG;
    
    ESP_LOGI(TAG, "Loading TOS ROM: %s", path);
    
    FILE *f = fopen(path, "rb");
    if (!f) {
        ESP_LOGE(TAG, "Failed to open ROM file: %s", path);
        return ESP_ERR_NOT_FOUND;
    }
    
    /* Get file size */
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    if (size <= 0 || size > ST_ROM_MAX_SIZE) {
        ESP_LOGE(TAG, "Invalid ROM size: %ld (max %d)", size, ST_ROM_MAX_SIZE);
        fclose(f);
        return ESP_ERR_INVALID_SIZE;
    }
    
    /* Free existing ROM if any */
    if (s_mem.rom) {
        heap_caps_free(s_mem.rom);
    }
    
    /* Allocate ROM buffer from PSRAM */
    s_mem.rom = heap_caps_malloc(size, MALLOC_CAP_SPIRAM);
    if (!s_mem.rom) {
        ESP_LOGE(TAG, "Failed to allocate ROM buffer (%ld bytes)", size);
        fclose(f);
        return ESP_ERR_NO_MEM;
    }
    
    /* Read ROM */
    size_t read_size = fread(s_mem.rom, 1, size, f);
    fclose(f);
    
    if (read_size != (size_t)size) {
        ESP_LOGE(TAG, "ROM read error: expected %ld, got %zu", size, read_size);
        heap_caps_free(s_mem.rom);
        s_mem.rom = NULL;
        return ESP_ERR_INVALID_SIZE;
    }
    
    s_mem.rom_size = (uint32_t)size;
    
    /* Determine ROM base from size:
     * 192KB TOS -> $FC0000
     * 256KB TOS -> $E00000 (later TOS versions)
     */
    if (size <= 192 * 1024) {
        s_mem.rom_base = 0xFC0000;
    } else {
        s_mem.rom_base = 0xE00000;
    }
    
    ESP_LOGI(TAG, "TOS ROM loaded: %luKB at $%06" PRIX32,
             (unsigned long)(size / 1024), s_mem.rom_base);
    
    /* Verify TOS header magic (first word should be a BRA instruction) */
    if (s_mem.rom_size >= 2) {
        uint16_t magic = ((uint16_t)s_mem.rom[0] << 8) | s_mem.rom[1];
        /* TOS starts with BRA.S (60xx) or BRA.W (6000) */
        if ((magic & 0xFF00) == 0x6000) {
            ESP_LOGI(TAG, "TOS header BRA: $%04X (valid)", magic);
        } else {
            ESP_LOGW(TAG, "TOS header: $%04X (unexpected, expected BRA)", magic);
        }
    }
    
    return ESP_OK;
}

esp_err_t st_memory_load_cartridge(const char *path)
{
    if (!path) return ESP_ERR_INVALID_ARG;
    
    ESP_LOGI(TAG, "Loading cartridge: %s", path);
    
    FILE *f = fopen(path, "rb");
    if (!f) return ESP_ERR_NOT_FOUND;
    
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    if (size <= 0 || size > 128 * 1024) {
        fclose(f);
        return ESP_ERR_INVALID_SIZE;
    }
    
    if (s_mem.cartridge) heap_caps_free(s_mem.cartridge);
    
    s_mem.cartridge = heap_caps_malloc(size, MALLOC_CAP_SPIRAM);
    if (!s_mem.cartridge) {
        fclose(f);
        return ESP_ERR_NO_MEM;
    }
    
    fread(s_mem.cartridge, 1, size, f);
    fclose(f);
    
    s_mem.cart_size = (uint32_t)size;
    
    ESP_LOGI(TAG, "Cartridge loaded: %luKB at $FA0000",
             (unsigned long)(size / 1024));
    
    return ESP_OK;
}

esp_err_t st_memory_register_io(const io_handler_t *handler)
{
    if (!handler) return ESP_ERR_INVALID_ARG;
    
    if (s_mem.io_handler_count >= ST_MAX_IO_HANDLERS) {
        ESP_LOGE(TAG, "I/O handler table full");
        return ESP_ERR_NO_MEM;
    }
    
    /* Verify address range is in I/O space */
    if (handler->base < ST_IO_BASE || handler->end < handler->base) {
        ESP_LOGE(TAG, "I/O handler '%s' has invalid range: $%06" PRIX32 "-$%06" PRIX32,
                 handler->name ? handler->name : "??",
                 handler->base, handler->end);
        return ESP_ERR_INVALID_ARG;
    }
    
    s_mem.io_handlers[s_mem.io_handler_count] = *handler;
    s_mem.io_handler_count++;
    
    ESP_LOGI(TAG, "Registered I/O handler '%s': $%06" PRIX32 "-$%06" PRIX32,
             handler->name ? handler->name : "??",
             handler->base, handler->end);
    
    return ESP_OK;
}

bus_interface_t* st_memory_get_bus(void)
{
    return &s_bus;
}

uint8_t* st_memory_get_ram(void)
{
    return s_mem.ram;
}

uint32_t st_memory_get_ram_size(void)
{
    return s_mem.ram_size;
}

uint8_t* st_memory_get_rom(void)
{
    return s_mem.rom;
}

uint32_t st_memory_get_rom_size(void)
{
    return s_mem.rom_size;
}

void st_memory_reset(void)
{
    /* Clear RAM (TOS expects this) */
    if (s_mem.ram) {
        memset(s_mem.ram, 0, s_mem.ram_size);
    }
    
    /* Reset MMU config */
    s_mem.mmu_config = 0;
    
    /* Reset statistics */
    s_mem.reads = 0;
    s_mem.writes = 0;
    s_mem.bus_errors = 0;
    
    ESP_LOGI(TAG, "Memory reset (RAM cleared, ROM preserved)");
}
