/**
 * @file esptari_input.h
 * @brief Input handling — keyboard, mouse, joystick from web interface
 *
 * SPDX-License-Identifier: MIT
 */
#pragma once

#include "esp_err.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Atari ST key scan codes (IKBD protocol) */
typedef uint8_t esptari_scancode_t;

/** Mouse state */
typedef struct {
    int16_t  dx;        /**< Relative X movement */
    int16_t  dy;        /**< Relative Y movement */
    bool     left;      /**< Left button */
    bool     right;     /**< Right button */
} esptari_mouse_t;

/** Joystick state (directly mapped to IKBD joystick bytes) */
typedef struct {
    bool up;
    bool down;
    bool left;
    bool right;
    bool fire;
} esptari_joystick_t;

/**
 * @brief Initialize input subsystem
 * @return ESP_OK on success
 */
esp_err_t esptari_input_init(void);

/**
 * @brief Inject a key press/release
 * @param scancode Atari IKBD scan code
 * @param pressed  true=press, false=release
 */
void esptari_input_key(esptari_scancode_t scancode, bool pressed);

/**
 * @brief Inject mouse movement
 */
void esptari_input_mouse(const esptari_mouse_t *mouse);

/**
 * @brief Inject joystick state (port 0 or 1)
 */
void esptari_input_joystick(int port, const esptari_joystick_t *joy);

/**
 * @brief Shut down input subsystem
 */
void esptari_input_deinit(void);

#ifdef __cplusplus
}
#endif
