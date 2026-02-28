/**
 * @file ym2149_internal.h
 * @brief YM2149 PSG Internal Definitions
 * 
 * The YM-2149 (General Instrument AY-3-8910 compatible) Programmable
 * Sound Generator provides:
 *   - 3 square wave tone channels (A, B, C)
 *   - 1 noise generator
 *   - Amplitude envelope generator
 *   - 2 bidirectional 8-bit I/O ports
 * 
 * In the Atari ST:
 *   - Port A: Output - FDC side select, sound enable, parallel strobe, etc.
 *   - Port B: Input/Output - Parallel port data
 *   - Clock: 2 MHz (CPU clock / 4)
 * 
 * Register Access via $FF8800-$FF8803:
 *   $FF8800 read:  Read selected register
 *   $FF8800 write: Select register (0-15)
 *   $FF8802 write: Write to selected register
 * 
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>

/*===========================================================================*/
/* YM2149 Registers                                                          */
/*===========================================================================*/

#define YM_REG_FREQ_A_LO    0   /**< Channel A frequency low (8 bits) */
#define YM_REG_FREQ_A_HI    1   /**< Channel A frequency high (4 bits) */
#define YM_REG_FREQ_B_LO    2   /**< Channel B frequency low */
#define YM_REG_FREQ_B_HI    3   /**< Channel B frequency high */
#define YM_REG_FREQ_C_LO    4   /**< Channel C frequency low */
#define YM_REG_FREQ_C_HI    5   /**< Channel C frequency high */
#define YM_REG_FREQ_NOISE   6   /**< Noise frequency (5 bits) */
#define YM_REG_MIXER        7   /**< Mixer control (tone/noise enable) */
#define YM_REG_AMP_A        8   /**< Channel A amplitude (4 bits + envelope) */
#define YM_REG_AMP_B        9   /**< Channel B amplitude */
#define YM_REG_AMP_C       10   /**< Channel C amplitude */
#define YM_REG_ENV_LO      11   /**< Envelope period low */
#define YM_REG_ENV_HI      12   /**< Envelope period high */
#define YM_REG_ENV_SHAPE   13   /**< Envelope shape */
#define YM_REG_PORT_A      14   /**< I/O Port A data */
#define YM_REG_PORT_B      15   /**< I/O Port B data */

#define YM_NUM_REGS        16

/*===========================================================================*/
/* Mixer Register Bits                                                       */
/*===========================================================================*/

#define YM_MIX_TONE_A      (1 << 0)  /**< 1 = tone A disabled */
#define YM_MIX_TONE_B      (1 << 1)  /**< 1 = tone B disabled */
#define YM_MIX_TONE_C      (1 << 2)  /**< 1 = tone C disabled */
#define YM_MIX_NOISE_A     (1 << 3)  /**< 1 = noise A disabled */
#define YM_MIX_NOISE_B     (1 << 4)  /**< 1 = noise B disabled */
#define YM_MIX_NOISE_C     (1 << 5)  /**< 1 = noise C disabled */
#define YM_MIX_PORT_A_OUT  (1 << 6)  /**< 1 = port A output */
#define YM_MIX_PORT_B_OUT  (1 << 7)  /**< 1 = port B output */

/*===========================================================================*/
/* Atari ST Port A Bits                                                      */
/*===========================================================================*/

#define YM_PA_DRIVE_SIDE    (1 << 0)  /**< FDC side select (active low) */
#define YM_PA_DRIVE_A       (1 << 1)  /**< Drive A select (active low) */
#define YM_PA_DRIVE_B       (1 << 2)  /**< Drive B select (active low) */
#define YM_PA_RTS           (1 << 3)  /**< RS-232 RTS */
#define YM_PA_DTR           (1 << 4)  /**< RS-232 DTR */
#define YM_PA_STROBE        (1 << 5)  /**< Centronics strobe */
#define YM_PA_MONO          (1 << 6)  /**< Monitor type: 0=mono, 1=color */
#define YM_PA_NOT_USED      (1 << 7)  /**< Internal pull-up */

/*===========================================================================*/
/* Tone Channel State                                                        */
/*===========================================================================*/

typedef struct {
    uint16_t period;     /**< Tone period (12-bit) */
    uint16_t counter;    /**< Period counter */
    uint8_t  output;     /**< Current output (0 or 1) */
} ym_channel_t;

/*===========================================================================*/
/* Envelope State                                                            */
/*===========================================================================*/

typedef struct {
    uint16_t period;     /**< Envelope period (16-bit) */
    uint16_t counter;    /**< Period counter */
    uint8_t  shape;      /**< Shape register */
    uint8_t  step;       /**< Current step (0-31) */
    uint8_t  volume;     /**< Current envelope volume (0-15) */
    bool     holding;    /**< Envelope holding at final value */
} ym_envelope_t;

/*===========================================================================*/
/* YM2149 State                                                              */
/*===========================================================================*/

typedef struct {
    /* Registers */
    uint8_t regs[YM_NUM_REGS];
    
    /* Selected register for read/write */
    uint8_t selected_reg;
    
    /* Tone channels */
    ym_channel_t channel[3];
    
    /* Noise generator */
    uint8_t  noise_period;
    uint8_t  noise_counter;
    uint32_t noise_rng;      /**< LFSR state */
    uint8_t  noise_output;   /**< Current noise output */
    
    /* Envelope */
    ym_envelope_t envelope;
    
    /* Port A callback (for FDC control, etc.) */
    void (*port_a_write)(uint8_t val, void *ctx);
    uint8_t (*port_a_read)(void *ctx);
    void *port_a_ctx;
    
    /* Port B callback */
    void (*port_b_write)(uint8_t val, void *ctx);
    uint8_t (*port_b_read)(void *ctx);
    void *port_b_ctx;
    
} ym2149_state_t;
