/**
 * @file st_acia.c
 * @brief Minimal ACIA 6850 Implementation for TOS Boot
 * 
 * Stub implementation of the keyboard and MIDI ACIA chips.
 * TOS reads the keyboard ACIA status during boot; if it never
 * sees TDRE (transmit data register empty) it will hang.
 * 
 * SPDX-License-Identifier: MIT
 */

#include <string.h>
#include "esp_log.h"
#include "st_acia.h"

static const char *TAG = "st_acia";

/*===========================================================================*/
/* Module State                                                              */
/*===========================================================================*/

static acia_state_t s_kbd_acia;
static acia_state_t s_midi_acia;

/*===========================================================================*/
/* I/O Handler Callbacks                                                     */
/*===========================================================================*/

static uint8_t kbd_acia_read_byte(uint32_t addr, void *ctx)
{
    (void)ctx;
    uint32_t offset = addr - IO_KBD_ACIA_BASE;
    
    switch (offset) {
        case 0:
        case 1:
            /* Status register */
            return s_kbd_acia.status;
            
        case 2:
        case 3: {
            /* Data register - read from receive buffer */
            uint8_t data = 0xFF;
            if (s_kbd_acia.rx_head != s_kbd_acia.rx_tail) {
                data = s_kbd_acia.rx_buf[s_kbd_acia.rx_tail];
                s_kbd_acia.rx_tail = (s_kbd_acia.rx_tail + 1) & 0x0F;
                
                /* Update RDRF flag */
                if (s_kbd_acia.rx_head == s_kbd_acia.rx_tail) {
                    s_kbd_acia.status &= ~ACIA_SR_RDRF;
                }
            }
            return data;
        }
        
        default:
            return 0xFF;
    }
}

static uint16_t kbd_acia_read_word(uint32_t addr, void *ctx)
{
    return ((uint16_t)kbd_acia_read_byte(addr, ctx) << 8) |
           kbd_acia_read_byte(addr + 1, ctx);
}

static void kbd_acia_write_byte(uint32_t addr, uint8_t val, void *ctx)
{
    (void)ctx;
    uint32_t offset = addr - IO_KBD_ACIA_BASE;
    
    switch (offset) {
        case 0:
        case 1:
            /* Control register */
            if ((val & 0x03) == 0x03) {
                /* Master reset */
                s_kbd_acia.status = ACIA_SR_TDRE;
                s_kbd_acia.rx_head = 0;
                s_kbd_acia.rx_tail = 0;
                ESP_LOGD(TAG, "KBD ACIA master reset");
            }
            s_kbd_acia.control = val;
            break;
            
        case 2:
        case 3:
            /* Transmit data - send command to keyboard controller */
            s_kbd_acia.tx_data = val;
            ESP_LOGD(TAG, "KBD ACIA TX: $%02X", val);
            /* Auto-acknowledge: keep TDRE */
            s_kbd_acia.status |= ACIA_SR_TDRE;
            break;
    }
}

static void kbd_acia_write_word(uint32_t addr, uint16_t val, void *ctx)
{
    kbd_acia_write_byte(addr, (uint8_t)(val >> 8), ctx);
    kbd_acia_write_byte(addr + 1, (uint8_t)(val & 0xFF), ctx);
}

/* MIDI ACIA - minimal stub */

static uint8_t midi_acia_read_byte(uint32_t addr, void *ctx)
{
    (void)ctx;
    uint32_t offset = addr - IO_MIDI_ACIA_BASE;
    
    if (offset <= 1) {
        return s_midi_acia.status;
    }
    return 0xFF;
}

static uint16_t midi_acia_read_word(uint32_t addr, void *ctx)
{
    return ((uint16_t)midi_acia_read_byte(addr, ctx) << 8) |
           midi_acia_read_byte(addr + 1, ctx);
}

static void midi_acia_write_byte(uint32_t addr, uint8_t val, void *ctx)
{
    (void)ctx;
    uint32_t offset = addr - IO_MIDI_ACIA_BASE;
    
    if (offset <= 1) {
        if ((val & 0x03) == 0x03) {
            s_midi_acia.status = ACIA_SR_TDRE;
        }
        s_midi_acia.control = val;
    } else {
        s_midi_acia.status |= ACIA_SR_TDRE;
    }
}

static void midi_acia_write_word(uint32_t addr, uint16_t val, void *ctx)
{
    midi_acia_write_byte(addr, (uint8_t)(val >> 8), ctx);
    midi_acia_write_byte(addr + 1, (uint8_t)(val & 0xFF), ctx);
}

/*===========================================================================*/
/* Public API                                                                */
/*===========================================================================*/

void st_acia_init(void)
{
    memset(&s_kbd_acia, 0, sizeof(s_kbd_acia));
    memset(&s_midi_acia, 0, sizeof(s_midi_acia));
    
    /* TDRE set by default (transmitter ready) */
    s_kbd_acia.status = ACIA_SR_TDRE;
    s_midi_acia.status = ACIA_SR_TDRE;
    
    /* Register keyboard ACIA I/O handler */
    io_handler_t kbd_handler = {
        .base       = IO_KBD_ACIA_BASE,
        .end        = IO_KBD_ACIA_END,
        .read_byte  = kbd_acia_read_byte,
        .read_word  = kbd_acia_read_word,
        .write_byte = kbd_acia_write_byte,
        .write_word = kbd_acia_write_word,
        .context    = NULL,
        .name       = "KBD ACIA",
    };
    st_memory_register_io(&kbd_handler);
    
    /* Register MIDI ACIA I/O handler */
    io_handler_t midi_handler = {
        .base       = IO_MIDI_ACIA_BASE,
        .end        = IO_MIDI_ACIA_END,
        .read_byte  = midi_acia_read_byte,
        .read_word  = midi_acia_read_word,
        .write_byte = midi_acia_write_byte,
        .write_word = midi_acia_write_word,
        .context    = NULL,
        .name       = "MIDI ACIA",
    };
    st_memory_register_io(&midi_handler);
    
    ESP_LOGI(TAG, "ACIAs initialized (keyboard + MIDI)");
}

void st_acia_reset(void)
{
    s_kbd_acia.status = ACIA_SR_TDRE;
    s_kbd_acia.rx_head = 0;
    s_kbd_acia.rx_tail = 0;
    
    s_midi_acia.status = ACIA_SR_TDRE;
}

void st_acia_kbd_push(uint8_t byte)
{
    int next = (s_kbd_acia.rx_head + 1) & 0x0F;
    if (next != s_kbd_acia.rx_tail) {
        s_kbd_acia.rx_buf[s_kbd_acia.rx_head] = byte;
        s_kbd_acia.rx_head = next;
        s_kbd_acia.status |= ACIA_SR_RDRF;
    }
}
