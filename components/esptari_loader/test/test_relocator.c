/**
 * @file test_relocator.c
 * @brief Unit tests for EBIN relocator
 * 
 * SPDX-License-Identifier: MIT
 */

#include <string.h>
#include <stdlib.h>
#include "unity.h"
#include "esp_err.h"
#include "ebin_format.h"

/* Declaration for the public relocator function */
extern esp_err_t relocator_apply(const ebin_reloc_t *relocs, 
                                  size_t reloc_count,
                                  void *code_base,
                                  void *data_base,
                                  uint32_t code_size,
                                  uint32_t data_size);

/*===========================================================================*/
/* Test: Absolute Relocation                                                 */
/*===========================================================================*/

TEST_CASE("Relocator applies absolute relocation", "[relocator]")
{
    /* Create a code buffer with a pointer to relocate */
    uint8_t code[64];
    memset(code, 0, sizeof(code));
    
    /* Place an address at offset 4 that needs relocation */
    /* Original value: 0x00000100 (relative to base 0) */
    uint32_t original_addr = 0x00000100;
    memcpy(&code[4], &original_addr, sizeof(uint32_t));
    
    /* Create relocation entry */
    ebin_reloc_t reloc = {
        .offset = 4,
        .type = EBIN_RELOC_ABSOLUTE,
        .section = 0,  /* code section */
        .reserved = 0
    };
    
    /* Apply relocation - the load address will be the actual code address */
    esp_err_t err = relocator_apply(&reloc, 1, code, NULL, sizeof(code), 0);
    
    TEST_ASSERT_EQUAL(ESP_OK, err);
    
    /* After relocation: original + code base address */
    uint32_t relocated_addr;
    memcpy(&relocated_addr, &code[4], sizeof(uint32_t));
    
    /* The relocated address should be: original + (uintptr_t)code */
    uint32_t expected = original_addr + (uint32_t)(uintptr_t)code;
    TEST_ASSERT_EQUAL_HEX32(expected, relocated_addr);
}

/*===========================================================================*/
/* Test: Relocation Entry Structure                                          */
/*===========================================================================*/

TEST_CASE("Relocation entry fields are accessible", "[relocator]")
{
    ebin_reloc_t reloc = {
        .offset = 0x1234,
        .type = EBIN_RELOC_RELATIVE,
        .section = 1,
        .reserved = 0
    };
    
    TEST_ASSERT_EQUAL(0x1234, reloc.offset);
    TEST_ASSERT_EQUAL(EBIN_RELOC_RELATIVE, reloc.type);
    TEST_ASSERT_EQUAL(1, reloc.section);
}

/*===========================================================================*/
/* Test: Multiple Relocations                                                */
/*===========================================================================*/

TEST_CASE("Relocator handles multiple entries", "[relocator]")
{
    uint8_t code[64];
    memset(code, 0, sizeof(code));
    
    /* Place two addresses to relocate */
    uint32_t addr1 = 0x100;
    uint32_t addr2 = 0x200;
    memcpy(&code[0], &addr1, sizeof(uint32_t));
    memcpy(&code[16], &addr2, sizeof(uint32_t));
    
    ebin_reloc_t relocs[2] = {
        { .offset = 0,  .type = EBIN_RELOC_ABSOLUTE, .section = 0, .reserved = 0 },
        { .offset = 16, .type = EBIN_RELOC_ABSOLUTE, .section = 0, .reserved = 0 }
    };
    
    /* Apply all relocations at once */
    esp_err_t err = relocator_apply(relocs, 2, code, NULL, sizeof(code), 0);
    TEST_ASSERT_EQUAL(ESP_OK, err);
    
    uint32_t result1, result2;
    memcpy(&result1, &code[0], sizeof(uint32_t));
    memcpy(&result2, &code[16], sizeof(uint32_t));
    
    /* Results should be original + code base address */
    uint32_t code_base = (uint32_t)(uintptr_t)code;
    TEST_ASSERT_EQUAL_HEX32(addr1 + code_base, result1);
    TEST_ASSERT_EQUAL_HEX32(addr2 + code_base, result2);
}

/*===========================================================================*/
/* Test: Data Section Relocation                                             */
/*===========================================================================*/

TEST_CASE("Relocator handles data section", "[relocator]")
{
    uint8_t code[64];
    uint8_t data[64];
    memset(code, 0, sizeof(code));
    memset(data, 0, sizeof(data));
    
    /* Place an address in the data section */
    uint32_t addr = 0x500;
    memcpy(&data[8], &addr, sizeof(uint32_t));
    
    ebin_reloc_t reloc = {
        .offset = 8,
        .type = EBIN_RELOC_ABSOLUTE,
        .section = 1,  /* data section */
        .reserved = 0
    };
    
    esp_err_t err = relocator_apply(&reloc, 1, code, data, sizeof(code), sizeof(data));
    TEST_ASSERT_EQUAL(ESP_OK, err);
    
    uint32_t result;
    memcpy(&result, &data[8], sizeof(uint32_t));
    
    /* Result should be original + code base (load address) */
    uint32_t code_base = (uint32_t)(uintptr_t)code;
    TEST_ASSERT_EQUAL_HEX32(addr + code_base, result);
}

/*===========================================================================*/
/* Test: Zero Relocations                                                    */
/*===========================================================================*/

TEST_CASE("Relocator handles zero relocations", "[relocator]")
{
    uint8_t code[16];
    memset(code, 0xAA, sizeof(code));
    
    esp_err_t err = relocator_apply(NULL, 0, code, NULL, sizeof(code), 0);
    TEST_ASSERT_EQUAL(ESP_OK, err);
    
    /* Code should be unchanged */
    for (int i = 0; i < 16; i++) {
        TEST_ASSERT_EQUAL_HEX8(0xAA, code[i]);
    }
}

/*===========================================================================*/
/* Test: Out of Bounds Relocation                                            */
/*===========================================================================*/

TEST_CASE("Relocator rejects out of bounds offset", "[relocator]")
{
    uint8_t code[16];
    memset(code, 0, sizeof(code));
    
    ebin_reloc_t reloc = {
        .offset = 100,  /* Way out of bounds */
        .type = EBIN_RELOC_ABSOLUTE,
        .section = 0,
        .reserved = 0
    };
    
    esp_err_t err = relocator_apply(&reloc, 1, code, NULL, sizeof(code), 0);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, err);  /* Partial failure */
}