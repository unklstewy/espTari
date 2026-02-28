/**
 * @file test_ebin_parser.c
 * @brief Unit tests for EBIN parser
 * 
 * SPDX-License-Identifier: MIT
 */

#include <string.h>
#include "unity.h"
#include "ebin_format.h"
#include "esptari_loader.h"
#include "component_api.h"

/**
 * @brief Create a valid EBIN header for testing
 */
static void create_valid_header(ebin_header_t *header)
{
    memset(header, 0, sizeof(ebin_header_t));
    header->magic = EBIN_MAGIC;
    header->version = EBIN_VERSION;
    header->type = EBIN_TYPE_CPU;
    header->flags = 0;
    header->code_size = 1024;
    header->data_size = 256;
    header->bss_size = 128;
    header->entry_offset = 0;
    header->interface_version = CPU_INTERFACE_V1;
    header->min_ram = 4096;
    header->reloc_count = 0;
    header->reloc_offset = sizeof(ebin_header_t);
    header->code_offset = sizeof(ebin_header_t);
    header->data_offset = sizeof(ebin_header_t) + 1024;
    header->symbol_offset = 0;
    header->symbol_count = 0;
}

/*===========================================================================*/
/* Test: EBIN Magic Number Validation                                        */
/*===========================================================================*/

TEST_CASE("EBIN magic number is correct value", "[ebin]")
{
    /* "EBIN" in little-endian = 0x4E494245 */
    TEST_ASSERT_EQUAL_HEX32(0x4E494245, EBIN_MAGIC);
    
    /* Verify by constructing from characters */
    uint32_t expected = ('N' << 24) | ('I' << 16) | ('B' << 8) | 'E';
    TEST_ASSERT_EQUAL_HEX32(expected, EBIN_MAGIC);
}

/*===========================================================================*/
/* Test: Header Size                                                         */
/*===========================================================================*/

TEST_CASE("EBIN header size is correct", "[ebin]")
{
    /* Header is 60 bytes (packed structure) */
    TEST_ASSERT_EQUAL(60, sizeof(ebin_header_t));
    TEST_ASSERT_EQUAL(60, EBIN_HEADER_SIZE);
}

/*===========================================================================*/
/* Test: Relocation Entry Size                                               */
/*===========================================================================*/

TEST_CASE("EBIN relocation entry is 8 bytes", "[ebin]")
{
    TEST_ASSERT_EQUAL(8, sizeof(ebin_reloc_t));
}

/*===========================================================================*/
/* Test: Valid Header Creation                                               */
/*===========================================================================*/

TEST_CASE("Create valid EBIN header", "[ebin]")
{
    ebin_header_t header;
    create_valid_header(&header);
    
    TEST_ASSERT_EQUAL_HEX32(EBIN_MAGIC, header.magic);
    TEST_ASSERT_EQUAL(EBIN_VERSION, header.version);
    TEST_ASSERT_EQUAL(EBIN_TYPE_CPU, header.type);
    TEST_ASSERT_EQUAL(1024, header.code_size);
    TEST_ASSERT_EQUAL(256, header.data_size);
    TEST_ASSERT_EQUAL(128, header.bss_size);
}

/*===========================================================================*/
/* Test: Component Types                                                     */
/*===========================================================================*/

TEST_CASE("Component types match between header and loader", "[ebin]")
{
    TEST_ASSERT_EQUAL(COMPONENT_TYPE_CPU, EBIN_TYPE_CPU);
    TEST_ASSERT_EQUAL(COMPONENT_TYPE_VIDEO, EBIN_TYPE_VIDEO);
    TEST_ASSERT_EQUAL(COMPONENT_TYPE_AUDIO, EBIN_TYPE_AUDIO);
    TEST_ASSERT_EQUAL(COMPONENT_TYPE_IO, EBIN_TYPE_IO);
}

/*===========================================================================*/
/* Test: Header Field Offsets (verify packed structure)                      */
/*===========================================================================*/

TEST_CASE("EBIN header fields are correctly packed", "[ebin]")
{
    ebin_header_t header;
    uint8_t *base = (uint8_t *)&header;
    
    /* Verify field offsets */
    TEST_ASSERT_EQUAL(0, (uint8_t *)&header.magic - base);
    TEST_ASSERT_EQUAL(4, (uint8_t *)&header.version - base);
    TEST_ASSERT_EQUAL(6, (uint8_t *)&header.type - base);
    TEST_ASSERT_EQUAL(8, (uint8_t *)&header.flags - base);
    TEST_ASSERT_EQUAL(12, (uint8_t *)&header.code_size - base);
    TEST_ASSERT_EQUAL(16, (uint8_t *)&header.data_size - base);
    TEST_ASSERT_EQUAL(20, (uint8_t *)&header.bss_size - base);
    TEST_ASSERT_EQUAL(24, (uint8_t *)&header.entry_offset - base);
    TEST_ASSERT_EQUAL(28, (uint8_t *)&header.interface_version - base);
    TEST_ASSERT_EQUAL(32, (uint8_t *)&header.min_ram - base);
    TEST_ASSERT_EQUAL(36, (uint8_t *)&header.reloc_count - base);
    TEST_ASSERT_EQUAL(40, (uint8_t *)&header.reloc_offset - base);
    TEST_ASSERT_EQUAL(44, (uint8_t *)&header.code_offset - base);
    TEST_ASSERT_EQUAL(48, (uint8_t *)&header.data_offset - base);
    TEST_ASSERT_EQUAL(52, (uint8_t *)&header.symbol_offset - base);
}

/*===========================================================================*/
/* Test: Flag Definitions                                                    */
/*===========================================================================*/

TEST_CASE("EBIN flags are distinct bits", "[ebin]")
{
    TEST_ASSERT_EQUAL_HEX32(0x01, EBIN_FLAG_HAS_SYMBOLS);
    TEST_ASSERT_EQUAL_HEX32(0x02, EBIN_FLAG_DEBUG);
    TEST_ASSERT_EQUAL_HEX32(0x04, EBIN_FLAG_COMPRESSED);
    
    /* Verify no overlap */
    uint32_t all_flags = EBIN_FLAG_HAS_SYMBOLS | EBIN_FLAG_DEBUG | EBIN_FLAG_COMPRESSED;
    TEST_ASSERT_EQUAL_HEX32(0x07, all_flags);
}

/*===========================================================================*/
/* Test: Relocation Types                                                    */
/*===========================================================================*/

TEST_CASE("EBIN relocation types are sequential", "[ebin]")
{
    TEST_ASSERT_EQUAL(0, EBIN_RELOC_ABSOLUTE);
    TEST_ASSERT_EQUAL(1, EBIN_RELOC_RELATIVE);
    TEST_ASSERT_EQUAL(2, EBIN_RELOC_HIGH16);
    TEST_ASSERT_EQUAL(3, EBIN_RELOC_LOW16);
}

/*===========================================================================*/
/* Test: Loaded Component Structure                                          */
/*===========================================================================*/

TEST_CASE("ebin_loaded_t path buffer size", "[ebin]")
{
    ebin_loaded_t loaded;
    
    /* Path should accommodate reasonable file paths */
    TEST_ASSERT_GREATER_OR_EQUAL(64, sizeof(loaded.path));
}
