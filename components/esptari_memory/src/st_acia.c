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

static inline void acia_push_rx(acia_state_t *acia, uint8_t byte);

static void acia_kbd_push_startup_sequence(void)
{
    acia_push_rx(&s_kbd_acia, 0xF0);
    acia_push_rx(&s_kbd_acia, 0x01);
}

static inline void acia_refresh_irq(acia_state_t *acia)
{
    if (!acia) {
        return;
    }

    if ((acia->status & ACIA_SR_RDRF) != 0U) {
        acia->status |= ACIA_SR_IRQ;
    } else {
        acia->status &= (uint8_t)~ACIA_SR_IRQ;
    }
}

static inline void acia_push_rx(acia_state_t *acia, uint8_t byte)
{
    if (!acia) {
        return;
    }

    int next = (acia->rx_head + 1) & 0x0F;
    if (next == acia->rx_tail) {
        return;
    }

    acia->rx_buf[acia->rx_head] = byte;
    acia->rx_head = next;
    acia->status |= ACIA_SR_RDRF;
    acia_refresh_irq(acia);
}

/*===========================================================================*/
/* I/O Handler Callbacks                                                     */
/*===========================================================================*/

static uint8_t kbd_acia_read_byte(uint32_t addr, void *ctx)
{
    (void)ctx;
    uint32_t offset = addr - IO_KBD_ACIA_BASE;
    bool data_reg = (offset & 0x02U) != 0U;
    
    switch (data_reg ? 2 : 0) {
        case 0:
            /* Status register */
            return s_kbd_acia.status;
            
        case 2: {
            /* Data register - read from receive buffer */
            uint8_t data = 0xFF;
            if (s_kbd_acia.rx_head != s_kbd_acia.rx_tail) {
                data = s_kbd_acia.rx_buf[s_kbd_acia.rx_tail];
                s_kbd_acia.rx_tail = (s_kbd_acia.rx_tail + 1) & 0x0F;
                
                /* Update RDRF flag */
                if (s_kbd_acia.rx_head == s_kbd_acia.rx_tail) {
                    s_kbd_acia.status &= ~ACIA_SR_RDRF;
                }
                acia_refresh_irq(&s_kbd_acia);
            }
            return data;
        }
        
        default:
            return 0xFF;
    }
}

static uint16_t kbd_acia_read_word(uint32_t addr, void *ctx)
{
    uint32_t offset = addr - IO_KBD_ACIA_BASE;
    bool data_reg = (offset & 0x02U) != 0U;

    if (data_reg) {
        uint8_t data = kbd_acia_read_byte(addr, ctx);
        return ((uint16_t)data << 8) | data;
    }

    uint8_t status = kbd_acia_read_byte(addr, ctx);
    return ((uint16_t)status << 8) | status;
}

static void kbd_acia_write_byte(uint32_t addr, uint8_t val, void *ctx)
{
    (void)ctx;
    uint32_t offset = addr - IO_KBD_ACIA_BASE;
    bool data_reg = (offset & 0x02U) != 0U;
    
    switch (data_reg ? 2 : 0) {
        case 0:
            /* Control register */
            if ((val & 0x03) == 0x03) {
                /* Master reset */
                s_kbd_acia.status = ACIA_SR_TDRE | ACIA_SR_DCD | ACIA_SR_CTS;
                s_kbd_acia.rx_head = 0;
                s_kbd_acia.rx_tail = 0;
                acia_kbd_push_startup_sequence();
                ESP_LOGD(TAG, "KBD ACIA master reset");
            }
            s_kbd_acia.control = val;
            break;
            
        case 2:
            /* Transmit data - send command to keyboard controller */
            s_kbd_acia.tx_data = val;
            ESP_LOGD(TAG, "KBD ACIA TX: $%02X", val);

            if (val == 0x80) {
                /* IKBD reset command response sequence (minimal). */
                acia_kbd_push_startup_sequence();
            }

            /* Auto-acknowledge transmit complete */
            s_kbd_acia.status |= ACIA_SR_TDRE;
            acia_refresh_irq(&s_kbd_acia);
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

    if ((offset & 0x02U) == 0U) {
        return s_midi_acia.status;
    }
    return 0xFF;
}

static uint16_t midi_acia_read_word(uint32_t addr, void *ctx)
{
    uint8_t status = midi_acia_read_byte(addr, ctx);
    return ((uint16_t)status << 8) | status;
}

static void midi_acia_write_byte(uint32_t addr, uint8_t val, void *ctx)
{
    (void)ctx;
    uint32_t offset = addr - IO_MIDI_ACIA_BASE;

    if ((offset & 0x02U) == 0U) {
        if ((val & 0x03) == 0x03) {
            s_midi_acia.status = ACIA_SR_TDRE | ACIA_SR_DCD | ACIA_SR_CTS;
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
    s_kbd_acia.status = ACIA_SR_TDRE | ACIA_SR_DCD | ACIA_SR_CTS;
    s_midi_acia.status = ACIA_SR_TDRE | ACIA_SR_DCD | ACIA_SR_CTS;

    /* IKBD power-on self-test response sequence. */
    acia_kbd_push_startup_sequence();
    
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
    s_kbd_acia.status = ACIA_SR_TDRE | ACIA_SR_DCD | ACIA_SR_CTS;
    s_kbd_acia.rx_head = 0;
    s_kbd_acia.rx_tail = 0;
    
    s_midi_acia.status = ACIA_SR_TDRE | ACIA_SR_DCD | ACIA_SR_CTS;

    /* Re-announce IKBD ready after reset. */
    acia_kbd_push_startup_sequence();
}

void st_acia_kbd_push(uint8_t byte)
{
    acia_push_rx(&s_kbd_acia, byte);
}

void st_acia_get_debug(st_acia_debug_t *out)
{
    if (!out) {
        return;
    }

    int pending = s_kbd_acia.rx_head - s_kbd_acia.rx_tail;
    if (pending < 0) {
        pending += 16;
    }

    out->kbd_status = s_kbd_acia.status;
    out->kbd_control = s_kbd_acia.control;
    out->kbd_last_tx = s_kbd_acia.tx_data;
    out->kbd_rx_pending = (uint8_t)pending;
}
