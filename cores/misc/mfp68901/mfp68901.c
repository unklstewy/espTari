/**
 * @file mfp68901.c
 * @brief MFP 68901 Multi-Function Peripheral - EBIN Component
 * 
 * Motorola 68901 Multi-Function Peripheral emulation for Atari ST.
 * This is a loadable EBIN component implementing io_interface_t.
 * 
 * The MFP is critical for TOS boot — Timer C provides the 200Hz
 * system heartbeat, and the interrupt controller routes all peripheral
 * interrupts to the 68000 via the GLUE chip (IPL level 6).
 * 
 * Clocking:
 *   The MFP runs at 2.4576 MHz in the Atari ST. Timers divide this
 *   clock by a programmable prescaler (4/10/16/50/64/100/200) and
 *   then count down from the data register value.
 * 
 * Register access:
 *   On the Atari ST, MFP registers are at odd addresses only.
 *   $FFFA01 = GPIP, $FFFA03 = AER, etc. Even addresses return $FF.
 * 
 * SPDX-License-Identifier: MIT
 */

#include <stdint.h>
#include <stdbool.h>
#include "mfp68901_internal.h"

/* No libc in EBIN - provide our own memset */
static void *ebin_memset(void *s, int c, unsigned int n)
{
    unsigned char *p = (unsigned char *)s;
    while (n--) *p++ = (unsigned char)c;
    return s;
}

/*===========================================================================*/
/* Global State                                                              */
/*===========================================================================*/

static mfp_state_t g_mfp;

/*===========================================================================*/
/* Timer Implementation                                                      */
/*===========================================================================*/

void mfp_timer_update_prescale(mfp_timer_t *timer)
{
    int mode = timer->control & 0x07;
    timer->prescale = mfp_timer_prescale[mode];
}

/**
 * @brief Clock a timer, return true if it expired (reached zero)
 */
bool mfp_timer_clock(mfp_timer_t *timer, int cycles)
{
    if (timer->prescale == 0) {
        return false;  /* Timer stopped */
    }
    
    timer->accum += cycles;
    
    bool expired = false;
    
    while (timer->accum >= timer->prescale) {
        timer->accum -= timer->prescale;
        
        if (timer->counter == 0) {
            /* Reload from data register */
            timer->counter = timer->data;
            /* If data is 0, effectively /256 */
            if (timer->counter == 0) {
                timer->counter = 0;  /* Will fire again next tick */
            }
            expired = true;
        } else {
            timer->counter--;
            if (timer->counter == 0) {
                /* Reload */
                timer->counter = timer->data;
                expired = true;
            }
        }
    }
    
    return expired;
}

/*===========================================================================*/
/* Interrupt Controller                                                      */
/*===========================================================================*/

void mfp_trigger_interrupt(mfp_state_t *mfp, int source)
{
    if (source < 8) {
        /* Register B (sources 0-7) */
        uint8_t bit = 1 << source;
        if (mfp->ierb & bit) {
            mfp->iprb |= bit;
        }
    } else {
        /* Register A (sources 8-15) */
        uint8_t bit = 1 << (source - 8);
        if (mfp->iera & bit) {
            mfp->ipra |= bit;
        }
    }
}

void mfp_update_irq(mfp_state_t *mfp)
{
    /* IRQ is active if any unmasked pending interrupt exists
     * that isn't already in-service at a higher priority */
    uint8_t active_a = mfp->ipra & mfp->imra;
    uint8_t active_b = mfp->iprb & mfp->imrb;
    
    mfp->irq_out = (active_a || active_b);
}

/**
 * @brief Get the highest priority pending interrupt vector
 * 
 * Priority: Register A bit 7 (highest) down to Register B bit 0 (lowest)
 */
static int mfp_get_highest_pending(void)
{
    uint8_t active_a = g_mfp.ipra & g_mfp.imra;
    uint8_t active_b = g_mfp.iprb & g_mfp.imrb;
    
    /* Check register A first (higher priority), bit 7 = highest */
    for (int i = 7; i >= 0; i--) {
        if (active_a & (1 << i)) {
            /* Check if higher priority is in-service */
            uint8_t mask = ~((1 << i) - 1);  /* All bits >= i */
            if (!(g_mfp.isra & mask & ~(1 << i))) {
                return i + 8;  /* Source number */
            }
        }
    }
    
    /* Then register B */
    for (int i = 7; i >= 0; i--) {
        if (active_b & (1 << i)) {
            return i;  /* Source number */
        }
    }
    
    return -1;  /* No pending interrupt */
}

/*===========================================================================*/
/* Register Access                                                           */
/*===========================================================================*/

static uint8_t mfp_read_register(uint32_t addr)
{
    /* MFP registers are at odd addresses: $FFFA01, $FFFA03, ... $FFFA2F */
    uint32_t reg = addr & 0x3F;
    
    /* Even addresses return $FF */
    if ((reg & 1) == 0) {
        return 0xFF;
    }
    
    switch (reg) {
        case MFP_REG_GPIP:   return g_mfp.gpip;
        case MFP_REG_AER:    return g_mfp.aer;
        case MFP_REG_DDR:    return g_mfp.ddr;
        case MFP_REG_IERA:   return g_mfp.iera;
        case MFP_REG_IERB:   return g_mfp.ierb;
        case MFP_REG_IPRA:   return g_mfp.ipra;
        case MFP_REG_IPRB:   return g_mfp.iprb;
        case MFP_REG_ISRA:   return g_mfp.isra;
        case MFP_REG_ISRB:   return g_mfp.isrb;
        case MFP_REG_IMRA:   return g_mfp.imra;
        case MFP_REG_IMRB:   return g_mfp.imrb;
        case MFP_REG_VR:     return g_mfp.vr;
        case MFP_REG_TACR:   return g_mfp.timer_a.control;
        case MFP_REG_TBCR:   return g_mfp.timer_b.control;
        case MFP_REG_TCDCR: {
            /* Timer C control in bits 6-4, Timer D in bits 2-0 */
            return ((g_mfp.timer_c.control & 0x07) << 4) |
                   (g_mfp.timer_d.control & 0x07);
        }
        case MFP_REG_TADR:   return g_mfp.timer_a.counter;
        case MFP_REG_TBDR:   return g_mfp.timer_b.counter;
        case MFP_REG_TCDR:   return g_mfp.timer_c.counter;
        case MFP_REG_TDDR:   return g_mfp.timer_d.counter;
        case MFP_REG_SCR:    return g_mfp.scr;
        case MFP_REG_UCR:    return g_mfp.ucr;
        case MFP_REG_RSR:    return g_mfp.rsr;
        case MFP_REG_TSR:    return g_mfp.tsr;
        case MFP_REG_UDR:    return g_mfp.udr;
        default:
            return 0xFF;
    }
}

static void mfp_write_register(uint32_t addr, uint8_t val)
{
    uint32_t reg = addr & 0x3F;
    
    /* Even addresses are ignored */
    if ((reg & 1) == 0) {
        return;
    }
    
    switch (reg) {
        case MFP_REG_GPIP:
            /* Only output bits (DDR=1) can be written */
            g_mfp.gpip = (g_mfp.gpip & ~g_mfp.ddr) | (val & g_mfp.ddr);
            break;
            
        case MFP_REG_AER:
            g_mfp.aer = val;
            break;
            
        case MFP_REG_DDR:
            g_mfp.ddr = val;
            break;
            
        case MFP_REG_IERA:
            g_mfp.iera = val;
            /* Disable clears pending */
            g_mfp.ipra &= val;
            mfp_update_irq(&g_mfp);
            break;
            
        case MFP_REG_IERB:
            g_mfp.ierb = val;
            g_mfp.iprb &= val;
            mfp_update_irq(&g_mfp);
            break;
            
        case MFP_REG_IPRA:
            /* Writing 0 clears pending bits, writing 1 has no effect */
            g_mfp.ipra &= val;
            mfp_update_irq(&g_mfp);
            break;
            
        case MFP_REG_IPRB:
            g_mfp.iprb &= val;
            mfp_update_irq(&g_mfp);
            break;
            
        case MFP_REG_ISRA:
            /* Writing 0 clears in-service bits */
            g_mfp.isra &= val;
            mfp_update_irq(&g_mfp);
            break;
            
        case MFP_REG_ISRB:
            g_mfp.isrb &= val;
            mfp_update_irq(&g_mfp);
            break;
            
        case MFP_REG_IMRA:
            g_mfp.imra = val;
            mfp_update_irq(&g_mfp);
            break;
            
        case MFP_REG_IMRB:
            g_mfp.imrb = val;
            mfp_update_irq(&g_mfp);
            break;
            
        case MFP_REG_VR:
            g_mfp.vr = val;
            break;
            
        case MFP_REG_TACR:
            g_mfp.timer_a.control = val & 0x0F;
            mfp_timer_update_prescale(&g_mfp.timer_a);
            if ((val & 0x07) == 0) {
                g_mfp.timer_a.accum = 0;
            }
            break;
            
        case MFP_REG_TBCR:
            g_mfp.timer_b.control = val & 0x0F;
            mfp_timer_update_prescale(&g_mfp.timer_b);
            if ((val & 0x07) == 0) {
                g_mfp.timer_b.accum = 0;
            }
            break;
            
        case MFP_REG_TCDCR:
            /* Timer C control: bits 6-4, Timer D: bits 2-0 */
            g_mfp.timer_c.control = (val >> 4) & 0x07;
            g_mfp.timer_d.control = val & 0x07;
            mfp_timer_update_prescale(&g_mfp.timer_c);
            mfp_timer_update_prescale(&g_mfp.timer_d);
            break;
            
        case MFP_REG_TADR:
            g_mfp.timer_a.data = val;
            /* If timer stopped, also update counter */
            if (g_mfp.timer_a.prescale == 0) {
                g_mfp.timer_a.counter = val;
            }
            break;
            
        case MFP_REG_TBDR:
            g_mfp.timer_b.data = val;
            if (g_mfp.timer_b.prescale == 0) {
                g_mfp.timer_b.counter = val;
            }
            break;
            
        case MFP_REG_TCDR:
            g_mfp.timer_c.data = val;
            if (g_mfp.timer_c.prescale == 0) {
                g_mfp.timer_c.counter = val;
            }
            break;
            
        case MFP_REG_TDDR:
            g_mfp.timer_d.data = val;
            if (g_mfp.timer_d.prescale == 0) {
                g_mfp.timer_d.counter = val;
            }
            break;
            
        case MFP_REG_SCR:
            g_mfp.scr = val;
            break;
            
        case MFP_REG_UCR:
            g_mfp.ucr = val;
            break;
            
        case MFP_REG_RSR:
            /* Only bit 0 (RE) is writable */
            g_mfp.rsr = (g_mfp.rsr & 0xFE) | (val & 0x01);
            break;
            
        case MFP_REG_TSR:
            /* Various writable bits */
            g_mfp.tsr = val;
            break;
            
        case MFP_REG_UDR:
            g_mfp.udr = val;
            /* Mark transmit buffer full, then immediately empty (no real serial) */
            g_mfp.tsr |= 0x80;  /* Buffer empty (auto-complete) */
            break;
            
        default:
            break;
    }
}

/*===========================================================================*/
/* io_interface_t Implementation                                             */
/*===========================================================================*/

/**
 * @brief Simple config structure matching what the EBIN receives
 */
typedef struct {
    void *context;
} simple_io_config_t;

static int mfp_init(simple_io_config_t *config)
{
    (void)config;
    ebin_memset(&g_mfp, 0, sizeof(g_mfp));
    
    /* Power-on defaults */
    g_mfp.vr = 0x0F;    /* Vector base = $0F (vector $F0-$FF) */
    g_mfp.tsr = 0x80;   /* Transmit buffer empty */
    
    /* GPIO initial state (active-low accent for some pins) */
    g_mfp.gpip = 0xFF;  /* All inputs high */
    
    return 0; /* ESP_OK */
}

static void mfp_reset(void)
{
    /* Preserve config but clear volatile state */
    g_mfp.ipra = 0;
    g_mfp.iprb = 0;
    g_mfp.isra = 0;
    g_mfp.isrb = 0;
    g_mfp.irq_out = false;
    
    /* Reset timers */
    g_mfp.timer_a.control = 0;
    g_mfp.timer_a.accum = 0;
    g_mfp.timer_b.control = 0;
    g_mfp.timer_b.accum = 0;
    g_mfp.timer_c.control = 0;
    g_mfp.timer_c.accum = 0;
    g_mfp.timer_d.control = 0;
    g_mfp.timer_d.accum = 0;
    
    mfp_timer_update_prescale(&g_mfp.timer_a);
    mfp_timer_update_prescale(&g_mfp.timer_b);
    mfp_timer_update_prescale(&g_mfp.timer_c);
    mfp_timer_update_prescale(&g_mfp.timer_d);
    
    g_mfp.tsr = 0x80;
    g_mfp.gpip = 0xFF;
    g_mfp.vr = 0x0F;
}

static void mfp_shutdown(void)
{
    ebin_memset(&g_mfp, 0, sizeof(g_mfp));
}

static uint8_t mfp_io_read_byte(uint32_t addr)
{
    return mfp_read_register(addr);
}

static uint16_t mfp_io_read_word(uint32_t addr)
{
    /* Word reads return high byte as $FF, low byte as register */
    return 0xFF00 | mfp_read_register(addr + 1);
}

static void mfp_io_write_byte(uint32_t addr, uint8_t val)
{
    mfp_write_register(addr, val);
}

static void mfp_io_write_word(uint32_t addr, uint16_t val)
{
    /* Word writes: only the low byte goes to the register */
    mfp_write_register(addr + 1, (uint8_t)(val & 0xFF));
}

/**
 * @brief Advance MFP timers by the given number of MFP clock cycles
 * 
 * The MFP clock is 2.4576 MHz. The caller must convert CPU cycles
 * to MFP cycles before calling this (CPU runs at 8 MHz in ST,
 * so ratio is ~3.255:1).
 */
static void mfp_clock(int cycles)
{
    /* Clock Timer A */
    if (mfp_timer_clock(&g_mfp.timer_a, cycles)) {
        mfp_trigger_interrupt(&g_mfp, MFP_INT_TIMER_A);
    }
    
    /* Clock Timer B */
    if (mfp_timer_clock(&g_mfp.timer_b, cycles)) {
        mfp_trigger_interrupt(&g_mfp, MFP_INT_TIMER_B);
    }
    
    /* Clock Timer C */
    if (mfp_timer_clock(&g_mfp.timer_c, cycles)) {
        mfp_trigger_interrupt(&g_mfp, MFP_INT_TIMER_C);
    }
    
    /* Clock Timer D */
    if (mfp_timer_clock(&g_mfp.timer_d, cycles)) {
        mfp_trigger_interrupt(&g_mfp, MFP_INT_TIMER_D);
    }
    
    mfp_update_irq(&g_mfp);
}

static bool mfp_irq_pending(void)
{
    return g_mfp.irq_out;
}

static uint8_t mfp_get_vector(void)
{
    int source = mfp_get_highest_pending();
    if (source < 0) {
        /* Spurious interrupt - return vector base + 15 */
        return (g_mfp.vr & 0xF0) | 0x0F;
    }
    
    /* If S bit set in VR, set in-service */
    if (g_mfp.vr & 0x08) {
        if (source < 8) {
            g_mfp.isrb |= (1 << source);
        } else {
            g_mfp.isra |= (1 << (source - 8));
        }
    }
    
    /* Clear pending */
    if (source < 8) {
        g_mfp.iprb &= ~(1 << source);
    } else {
        g_mfp.ipra &= ~(1 << (source - 8));
    }
    
    mfp_update_irq(&g_mfp);
    
    /* Vector = base (bits 7-4) | source number (bits 3-0) */
    return (g_mfp.vr & 0xF0) | (source & 0x0F);
}

static void mfp_irq_ack(void)
{
    /* ACK handled in get_vector */
}

static void mfp_set_bus(void *bus)
{
    /* MFP doesn't need bus access (no DMA) */
    (void)bus;
}

static bool mfp_bus_held(void)
{
    return false;
}

/*===========================================================================*/
/* Interface Structure                                                       */
/*===========================================================================*/

static const struct {
    uint32_t interface_version;
    const char *name;
    
    int  (*init)(simple_io_config_t *config);
    void (*reset)(void);
    void (*shutdown)(void);
    
    uint8_t  (*read_byte)(uint32_t addr);
    uint16_t (*read_word)(uint32_t addr);
    void     (*write_byte)(uint32_t addr, uint8_t val);
    void     (*write_word)(uint32_t addr, uint16_t val);
    
    void (*clock)(int cycles);
    
    bool    (*irq_pending)(void);
    uint8_t (*get_vector)(void);
    void    (*irq_ack)(void);
    
    void (*set_bus)(void *bus);
    bool (*bus_held)(void);
} s_interface = {
    .interface_version = 0x00010000,  /* IO_INTERFACE_V1 */
    .name              = "MFP 68901",
    
    .init          = mfp_init,
    .reset         = mfp_reset,
    .shutdown      = mfp_shutdown,
    
    .read_byte     = mfp_io_read_byte,
    .read_word     = mfp_io_read_word,
    .write_byte    = mfp_io_write_byte,
    .write_word    = mfp_io_write_word,
    
    .clock         = mfp_clock,
    
    .irq_pending   = mfp_irq_pending,
    .get_vector    = mfp_get_vector,
    .irq_ack       = mfp_irq_ack,
    
    .set_bus       = mfp_set_bus,
    .bus_held      = mfp_bus_held,
};

/*===========================================================================*/
/* EBIN Entry Point                                                          */
/*===========================================================================*/

void* component_entry(void)
{
    return (void*)&s_interface;
}
