/**
 * @file st_acia.h
 * @brief Minimal ACIA 6850 stubs for Atari ST
 * 
 * The Atari ST has two ACIA 6850 chips:
 *   - Keyboard ACIA at $FFFC00-$FFFC06
 *   - MIDI ACIA at $FFFC20-$FFFC26
 * 
 * For Phase 2 (TOS boot), we just need minimal register stubs
 * so TOS doesn't hang waiting for the keyboard controller.
 * 
 * ACIA registers (each chip):
 *   offset 0: Control (write) / Status (read)
 *   offset 2: Data (read/write)
 * 
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <stdint.h>
#include "esptari_memory.h"

#ifdef __cplusplus
extern "C" {
#endif

/** ACIA status register bits */
#define ACIA_SR_RDRF    (1 << 0)  /**< Receive Data Register Full */
#define ACIA_SR_TDRE    (1 << 1)  /**< Transmit Data Register Empty */
#define ACIA_SR_DCD     (1 << 2)  /**< Data Carrier Detect */
#define ACIA_SR_CTS     (1 << 3)  /**< Clear To Send */
#define ACIA_SR_FE      (1 << 4)  /**< Framing Error */
#define ACIA_SR_OVRN    (1 << 5)  /**< Overrun */
#define ACIA_SR_PE      (1 << 6)  /**< Parity Error */
#define ACIA_SR_IRQ     (1 << 7)  /**< Interrupt Request */

typedef struct {
    uint8_t control;
    uint8_t status;
    uint8_t rx_data;
    uint8_t tx_data;
    
    /* Receive buffer (small ring) */
    uint8_t rx_buf[16];
    int rx_head;
    int rx_tail;
} acia_state_t;

/**
 * @brief Initialize ACIA stubs and register I/O handlers
 */
void st_acia_init(void);

/**
 * @brief Reset ACIA state
 */
void st_acia_reset(void);

/**
 * @brief Push a byte into the keyboard ACIA receive buffer
 * 
 * Used by the keyboard controller emulation to send scan codes.
 */
void st_acia_kbd_push(uint8_t byte);

#ifdef __cplusplus
}
#endif
