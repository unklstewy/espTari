/**
 * @file esptari_input.c
 * @brief Input handling — keyboard, mouse, joystick
 *
 * SPDX-License-Identifier: MIT
 */
#include "esptari_input.h"
#include "esp_log.h"

static const char *TAG = "esptari_input";

esp_err_t esptari_input_init(void)
{
    ESP_LOGI(TAG, "Input subsystem initialized");
    /* TODO: Set up IKBD input queue */
    return ESP_OK;
}

void esptari_input_key(esptari_scancode_t scancode, bool pressed)
{
    (void)scancode; (void)pressed;
    /* TODO: Enqueue key event to IKBD ACIA */
}

void esptari_input_mouse(const esptari_mouse_t *mouse)
{
    (void)mouse;
    /* TODO: Enqueue relative mouse packet to IKBD */
}

void esptari_input_joystick(int port, const esptari_joystick_t *joy)
{
    (void)port; (void)joy;
    /* TODO: Update joystick register */
}

void esptari_input_deinit(void)
{
    ESP_LOGI(TAG, "Input subsystem shut down");
}
