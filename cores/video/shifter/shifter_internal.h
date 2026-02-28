/**
 * @file shifter_internal.h
 * @brief Atari ST Shifter Video Controller Internal Definitions
 * 
 * The Shifter converts the ST's planar bitplane video RAM into
 * pixel data driven by the color palette. In Phase 2 this handles
 * register access and timing; rendering comes in Phase 3.
 * 
 * Register Map ($FF8200-$FF8260):
 *   $FF8201  Video base address high byte
 *   $FF8203  Video base address mid byte
 *   $FF8205  Video counter high byte (read-only)
 *   $FF8207  Video counter mid byte (read-only) 
 *   $FF8209  Video counter low byte (read-only)
 *   $FF820D  Video base address low byte (STe only)
 *   $FF8240-$FF825F  Palette registers (16 x 16-bit)
 *   $FF8260  Resolution register
 * 
 * Video Modes:
 *   Bit 0-1 of $FF8260:
 *     0 = Low res:  320x200, 16 colors, 4 bitplanes
 *     1 = Med res:  640x200,  4 colors, 2 bitplanes
 *     2 = High res: 640x400,  2 colors, 1 bitplane (mono)
 * 
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>

/*===========================================================================*/
/* Shifter Register Offsets (from $FF8200)                                   */
/*===========================================================================*/

/** Video base address registers */
#define SHIFT_REG_BASE_HI      0x01    /**< $FF8201 - Base high byte */
#define SHIFT_REG_BASE_MID     0x03    /**< $FF8203 - Base mid byte */
#define SHIFT_REG_BASE_LO      0x0D    /**< $FF820D - Base low byte (STe) */

/** Video counter registers (read-only) */
#define SHIFT_REG_COUNT_HI     0x05    /**< $FF8205 */
#define SHIFT_REG_COUNT_MID    0x07    /**< $FF8207 */
#define SHIFT_REG_COUNT_LO     0x09    /**< $FF8209 */

/** Sync mode */
#define SHIFT_REG_SYNC         0x0A    /**< $FF820A */

/** Line width (STe) */
#define SHIFT_REG_LINEWIDTH    0x0F    /**< $FF820F (STe) */

/** Palette registers */
#define SHIFT_REG_PALETTE      0x40    /**< $FF8240-$FF825F */
#define SHIFT_REG_PALETTE_END  0x5F

/** Resolution */
#define SHIFT_REG_RES          0x60    /**< $FF8260 */

/*===========================================================================*/
/* Resolution Constants                                                      */
/*===========================================================================*/

#define ST_RES_LOW    0   /**< 320x200, 16 colors */
#define ST_RES_MED    1   /**< 640x200, 4 colors */
#define ST_RES_HIGH   2   /**< 640x400, 2 colors (mono) */

/*===========================================================================*/
/* Shifter State                                                             */
/*===========================================================================*/

typedef struct {
    /* Video base address (set by CPU, latched at VBL) */
    uint32_t video_base;     /**< Video base address ($FF8201/03/0D) */
    
    /* Video counter (incremented by DMA during display) */
    uint32_t video_counter;  /**< Current video DMA address */
    
    /* Resolution */
    uint8_t  resolution;     /**< 0=low, 1=med, 2=high */
    
    /* Sync mode */
    uint8_t  sync_mode;      /**< Bit 1: 0=internal, 1=external */
    
    /* Palette (ST format: ----rrr-ggg-bbb-) */
    uint16_t palette[16];
    
    /* Converted palette (RGB565 for fast rendering) */
    uint16_t palette_rgb565[16];
    
    /* Line width offset (STe: extra bytes per line) */
    uint8_t  line_width;
    
    /* Bus interface for reading video RAM */
    uint8_t  (*bus_read_byte)(uint32_t addr);
    uint16_t (*bus_read_word)(uint32_t addr);
    void     *bus_context;
    
} shifter_state_t;

/*===========================================================================*/
/* Internal Functions                                                        */
/*===========================================================================*/

/** Convert ST palette entry to RGB565 */
uint16_t shifter_st_to_rgb565(uint16_t st_color);

/** Update all palette RGB565 values */
void shifter_update_palette(shifter_state_t *s);
