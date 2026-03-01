/**
 * @file esptari_memory.h
 * @brief Atari ST Memory Map and Bus Controller
 * 
 * Implements the 16MB Atari ST address space, routing CPU bus accesses
 * to RAM, ROM, and I/O chip handlers. This is the central interconnect
 * between the CPU and all other hardware components.
 * 
 * Memory Map:
 *   $000000-$0007FF  Exception vectors & system variables
 *   $000800-$DFFFFF  RAM (up to 14MB, typically 512KB-4MB)
 *   $E00000-$EFFFFF  Reserved (some clones/expansions use this)
 *   $F00000-$FBFFFF  Reserved
 *   $FC0000-$FEFFFF  TOS ROM (192KB standard, up to 256KB)
 *   $FF0000-$FF7FFF  Reserved I/O (cartridge area on some models)
 *   $FF8000-$FFFFFF  I/O Space
 * 
 * I/O Space Detail ($FF8000-$FFFFFF):
 *   $FF8001          MMU configuration register
 *   $FF8200-$FF820D  Shifter video registers
 *   $FF8240-$FF825F  Shifter palette (16 entries)
 *   $FF8260          Shifter resolution
 *   $FF8604-$FF860F  FDC/HDC (WD1772 + DMA)
 *   $FF8800-$FF8803  YM2149 PSG
 *   $FF8900-$FF893F  DMA Sound (STe/TT/Falcon)
 *   $FF8A00-$FF8A3F  Blitter
 *   $FFFA00-$FFFA3F  MFP 68901
 *   $FFFC00-$FFFC06  ACIA 6850 (Keyboard)
 *   $FFFC20-$FFFC26  ACIA 6850 (MIDI)
 * 
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "component_api.h"

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================*/
/* Address Space Constants                                                   */
/*===========================================================================*/

/** Maximum RAM size (14MB - TOS may limit to 4MB/14MB depending on version) */
#define ST_RAM_MAX          (14 * 1024 * 1024)

/** Default RAM size (4MB - Atari STe) */
#define ST_RAM_DEFAULT      (4 * 1024 * 1024)

/** TOS ROM base address */
#define ST_ROM_BASE         0xFC0000

/** TOS ROM maximum size (256KB) */
#define ST_ROM_MAX_SIZE     (256 * 1024)

/** I/O space base */
#define ST_IO_BASE          0xFF8000

/** I/O space size */
#define ST_IO_SIZE          0x8000

/*===========================================================================*/
/* I/O Address Ranges                                                        */
/*===========================================================================*/

/** MMU configuration */
#define IO_MMU_CONFIG       0xFF8001

/** Video (Shifter) registers */
#define IO_VIDEO_BASE       0xFF8200
#define IO_VIDEO_END        0xFF8260

/** Video palette */
#define IO_PALETTE_BASE     0xFF8240
#define IO_PALETTE_END      0xFF825F

/** FDC and DMA */
#define IO_FDC_BASE         0xFF8604
#define IO_FDC_END          0xFF860F

/** YM2149 PSG */
#define IO_PSG_BASE         0xFF8800
#define IO_PSG_END          0xFF8803

/** DMA Sound (STe) */
#define IO_DMA_SOUND_BASE   0xFF8900
#define IO_DMA_SOUND_END    0xFF893F

/** Blitter */
#define IO_BLITTER_BASE     0xFF8A00
#define IO_BLITTER_END      0xFF8A3F

/** MFP 68901 */
#define IO_MFP_BASE         0xFFFA00
#define IO_MFP_END          0xFFFA3F

/** Keyboard ACIA */
#define IO_KBD_ACIA_BASE    0xFFFC00
#define IO_KBD_ACIA_END     0xFFFC06

/** MIDI ACIA */
#define IO_MIDI_ACIA_BASE   0xFFFC20
#define IO_MIDI_ACIA_END    0xFFFC26

/*===========================================================================*/
/* I/O Handler Registration                                                  */
/*===========================================================================*/

/**
 * @brief I/O region handler for memory-mapped peripherals
 */
typedef struct {
    uint32_t base;              /**< Region start address */
    uint32_t end;               /**< Region end address (inclusive) */
    
    /** Read byte from I/O address */
    uint8_t  (*read_byte)(uint32_t addr, void *ctx);
    /** Read word from I/O address */
    uint16_t (*read_word)(uint32_t addr, void *ctx);
    /** Write byte to I/O address */
    void     (*write_byte)(uint32_t addr, uint8_t val, void *ctx);
    /** Write word to I/O address */
    void     (*write_word)(uint32_t addr, uint16_t val, void *ctx);
    
    void *context;              /**< Handler context (chip state) */
    const char *name;           /**< Handler name for debugging */
} io_handler_t;

/** Maximum number of registered I/O handlers */
#define ST_MAX_IO_HANDLERS  16

/*===========================================================================*/
/* ST Memory State                                                           */
/*===========================================================================*/

/**
 * @brief Atari ST memory subsystem state
 */
typedef struct {
    /* RAM */
    uint8_t  *ram;              /**< Main RAM (allocated from PSRAM) */
    uint32_t  ram_size;         /**< RAM size in bytes */
    
    /* ROM */
    uint8_t  *rom;              /**< TOS ROM (allocated from PSRAM) */
    uint32_t  rom_size;         /**< ROM size in bytes */
    uint32_t  rom_base;         /**< ROM base address */
    
    /* Cartridge ROM (optional) */
    uint8_t  *cartridge;        /**< Cartridge ROM */
    uint32_t  cart_size;        /**< Cartridge size */
    
    /* MMU */
    uint8_t   mmu_config;       /**< MMU configuration register (bank config) */
    
    /* I/O Handler Table */
    io_handler_t io_handlers[ST_MAX_IO_HANDLERS];
    int          io_handler_count;
    
    /* Statistics */
    uint64_t  reads;
    uint64_t  writes;
    uint64_t  bus_errors;
    
} st_memory_t;

/*===========================================================================*/
/* Public API                                                                */
/*===========================================================================*/

/**
 * @brief Initialize the ST memory subsystem
 * 
 * Allocates RAM from PSRAM, initializes the address decoder.
 * 
 * @param[in] ram_size  RAM size in bytes (e.g., 4*1024*1024 for 4MB)
 * @return ESP_OK on success
 */
esp_err_t st_memory_init(uint32_t ram_size);

/**
 * @brief Shutdown and free all memory
 */
void st_memory_shutdown(void);

/**
 * @brief Load TOS ROM from file
 * 
 * @param[in] path  Path to TOS ROM file (e.g., "/sdcard/roms/tos/tos206.img")
 * @return ESP_OK on success
 */
esp_err_t st_memory_load_rom(const char *path);

/**
 * @brief Load cartridge ROM from file
 * 
 * @param[in] path  Path to cartridge ROM file
 * @return ESP_OK on success
 */
esp_err_t st_memory_load_cartridge(const char *path);

/**
 * @brief Register an I/O handler for a memory-mapped device
 * 
 * @param[in] handler  I/O handler definition
 * @return ESP_OK on success, ESP_ERR_NO_MEM if table full
 */
esp_err_t st_memory_register_io(const io_handler_t *handler);

/**
 * @brief Get bus interface for CPU connection
 * 
 * Returns a bus_interface_t that the CPU uses to access the
 * Atari ST address space.
 * 
 * @return Pointer to bus interface (static, valid until shutdown)
 */
bus_interface_t* st_memory_get_bus(void);

/**
 * @brief Get pointer to raw RAM for direct access
 * 
 * Used by video DMA, blitter, etc. that need direct RAM access.
 * 
 * @return Pointer to RAM base
 */
uint8_t* st_memory_get_ram(void);

/**
 * @brief Get RAM size
 * @return RAM size in bytes
 */
uint32_t st_memory_get_ram_size(void);

/**
 * @brief Get pointer to ROM for direct access
 * @return Pointer to ROM base
 */
uint8_t* st_memory_get_rom(void);

/**
 * @brief Get ROM size
 * @return ROM size in bytes
 */
uint32_t st_memory_get_rom_size(void);

/**
 * @brief Reset the memory subsystem
 * 
 * Clears RAM, resets MMU config, does NOT unload ROM.
 */
void st_memory_reset(void);

/**
 * @brief Get memory bus statistics counters
 */
void st_memory_get_stats(uint64_t *reads, uint64_t *writes, uint64_t *bus_errors);

/**
 * @brief Get last bus/address error details captured by memory subsystem
 *
 * @param[out] addr  Last fault address (24-bit bus address)
 * @param[out] write True if fault came from write path, false for read path
 */
void st_memory_get_last_bus_error(uint32_t *addr, bool *write);

#ifdef __cplusplus
}
#endif
