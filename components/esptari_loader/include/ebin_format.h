/**
 * @file ebin_format.h
 * @brief EBIN (ESP Binary) file format definitions
 * 
 * The EBIN format is a simple binary format for position-independent code
 * that can be loaded and relocated at runtime into PSRAM.
 * 
 * File Structure:
 * ┌────────────────────────────────────────────────┐
 * │ EBIN Header (64 bytes)                         │
 * ├────────────────────────────────────────────────┤
 * │ Relocation Table                               │
 * ├────────────────────────────────────────────────┤
 * │ Code Section (Position Independent)            │
 * ├────────────────────────────────────────────────┤
 * │ Data Section                                   │
 * ├────────────────────────────────────────────────┤
 * │ Symbol Table (optional)                        │
 * └────────────────────────────────────────────────┘
 * 
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** EBIN magic number: "EBIN" in little-endian */
#define EBIN_MAGIC          0x4E494245  /* "EBIN" */

/** Current EBIN format version */
#define EBIN_VERSION        1

/** EBIN component types (matches component_type_t) */
#define EBIN_TYPE_CPU       1
#define EBIN_TYPE_VIDEO     2
#define EBIN_TYPE_AUDIO     3
#define EBIN_TYPE_IO        4
#define EBIN_TYPE_SYSTEM    5

/** EBIN flags */
#define EBIN_FLAG_HAS_SYMBOLS   (1 << 0)  /**< Symbol table present */
#define EBIN_FLAG_DEBUG         (1 << 1)  /**< Debug build */
#define EBIN_FLAG_COMPRESSED    (1 << 2)  /**< Code/data compressed */

/** Relocation types */
#define EBIN_RELOC_ABSOLUTE     0   /**< 32-bit absolute address */
#define EBIN_RELOC_RELATIVE     1   /**< 32-bit PC-relative */
#define EBIN_RELOC_HIGH16       2   /**< Upper 16 bits */
#define EBIN_RELOC_LOW16        3   /**< Lower 16 bits */

/**
 * @brief EBIN file header (64 bytes, fixed size)
 */
typedef struct __attribute__((packed)) {
    uint32_t magic;              /**< Magic number (EBIN_MAGIC) */
    uint16_t version;            /**< Format version */
    uint16_t type;               /**< Component type */
    uint32_t flags;              /**< Feature flags */
    uint32_t code_size;          /**< Code section size in bytes */
    uint32_t data_size;          /**< Data section size in bytes */
    uint32_t bss_size;           /**< BSS (zero-initialized) size */
    uint32_t entry_offset;       /**< Entry point offset from code start */
    uint32_t interface_version;  /**< Required interface version */
    uint32_t min_ram;            /**< Minimum working RAM required */
    uint32_t reloc_count;        /**< Number of relocation entries */
    uint32_t reloc_offset;       /**< Offset to relocation table */
    uint32_t code_offset;        /**< Offset to code section */
    uint32_t data_offset;        /**< Offset to data section */
    uint32_t symbol_offset;      /**< Offset to symbol table (0 if none) */
    uint32_t symbol_count;       /**< Number of symbols */
} ebin_header_t;

/* Verify header size at compile time - should be 64 bytes */
#define EBIN_HEADER_SIZE sizeof(ebin_header_t)

/**
 * @brief Relocation table entry (8 bytes)
 */
typedef struct __attribute__((packed)) {
    uint32_t offset;    /**< Offset within code/data section */
    uint8_t  type;      /**< Relocation type (EBIN_RELOC_*) */
    uint8_t  section;   /**< 0 = code, 1 = data */
    uint16_t reserved;  /**< Reserved */
} ebin_reloc_t;

_Static_assert(sizeof(ebin_reloc_t) == 8, "Relocation entry must be 8 bytes");

/**
 * @brief Symbol table entry (variable size due to name)
 */
typedef struct __attribute__((packed)) {
    uint32_t offset;      /**< Offset within section */
    uint8_t  section;     /**< Section: 0=code, 1=data, 2=bss */
    uint8_t  name_len;    /**< Length of symbol name */
    /* char name[name_len] follows */
} ebin_symbol_t;

/**
 * @brief Loaded component structure (runtime)
 */
typedef struct {
    ebin_header_t header;    /**< Copy of file header */
    void         *code_base; /**< Code section base in PSRAM */
    void         *data_base; /**< Data section base in PSRAM */
    void         *bss_base;  /**< BSS section base in PSRAM */
    void         *interface; /**< Component interface pointer */
    char          path[128]; /**< Path component was loaded from */
} ebin_loaded_t;

#ifdef __cplusplus
}
#endif
