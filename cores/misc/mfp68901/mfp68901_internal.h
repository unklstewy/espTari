/**
 * @file mfp68901_internal.h
 * @brief MFP 68901 Internal Definitions
 * 
 * Motorola MFP 68901 Multi-Function Peripheral register definitions
 * and internal state. The MFP provides:
 *   - 8 GPIO pins with edge detection
 *   - 16-source interrupt controller with individual masking
 *   - 4 programmable timers (A-D)
 *   - Full-duplex USART
 * 
 * In the Atari ST, the MFP is mapped at $FFFA00-$FFFA3F with
 * registers at odd addresses only (directly mapped to data bus D0-D7).
 * 
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>

/*===========================================================================*/
/* MFP Register Offsets (from base $FFFA00)                                  */
/* Note: On the ST, registers are at ODD addresses ($FFFA01, $FFFA03, etc.)  */
/* The offset values here are the byte offset >> 1 for indexing.             */
/*===========================================================================*/

/** GPIO */
#define MFP_REG_GPIP       0x01    /**< General Purpose I/O Data */
#define MFP_REG_AER        0x03    /**< Active Edge Register */
#define MFP_REG_DDR        0x05    /**< Data Direction Register */

/** Interrupt Controller */
#define MFP_REG_IERA       0x07    /**< Interrupt Enable Register A */
#define MFP_REG_IERB       0x09    /**< Interrupt Enable Register B */
#define MFP_REG_IPRA       0x0B    /**< Interrupt Pending Register A */
#define MFP_REG_IPRB       0x0D    /**< Interrupt Pending Register B */
#define MFP_REG_ISRA       0x0F    /**< Interrupt In-Service Register A */
#define MFP_REG_ISRB       0x11    /**< Interrupt In-Service Register B */
#define MFP_REG_IMRA       0x13    /**< Interrupt Mask Register A */
#define MFP_REG_IMRB       0x15    /**< Interrupt Mask Register B */
#define MFP_REG_VR         0x17    /**< Vector Register */

/** Timers */
#define MFP_REG_TACR       0x19    /**< Timer A Control */
#define MFP_REG_TBCR       0x1B    /**< Timer B Control */
#define MFP_REG_TCDCR      0x1D    /**< Timer C/D Control */
#define MFP_REG_TADR       0x1F    /**< Timer A Data */
#define MFP_REG_TBDR       0x21    /**< Timer B Data */
#define MFP_REG_TCDR       0x23    /**< Timer C Data */
#define MFP_REG_TDDR       0x25    /**< Timer D Data */

/** USART */
#define MFP_REG_SCR        0x27    /**< Sync Character */
#define MFP_REG_UCR        0x29    /**< USART Control */
#define MFP_REG_RSR        0x2B    /**< Receiver Status */
#define MFP_REG_TSR        0x2D    /**< Transmitter Status */
#define MFP_REG_UDR        0x2F    /**< USART Data */

/*===========================================================================*/
/* MFP Interrupt Sources (bit positions)                                     */
/*===========================================================================*/

/** Register B interrupts (bits 0-7) */
#define MFP_INT_GPIO0       0   /**< I0 - Centronics busy */
#define MFP_INT_GPIO1       1   /**< I1 - RS-232 DCD */
#define MFP_INT_GPIO2       2   /**< I2 - RS-232 CTS */
#define MFP_INT_GPIO3       3   /**< I3 - Blitter done */
#define MFP_INT_TIMER_D     4   /**< Timer D (RS-232 baud rate) */
#define MFP_INT_TIMER_C     5   /**< Timer C (200Hz system clock) */
#define MFP_INT_GPIO4       6   /**< I4 - Keyboard/MIDI ACIA */
#define MFP_INT_GPIO5       7   /**< I5 - FDC/HDC */

/** Register A interrupts (bits 8-15) */
#define MFP_INT_TIMER_B     8   /**< Timer B (HBL counter) */
#define MFP_INT_TX_ERROR    9   /**< Transmit error */
#define MFP_INT_TX_EMPTY   10   /**< Transmit buffer empty */
#define MFP_INT_RX_ERROR   11   /**< Receive error */
#define MFP_INT_RX_FULL    12   /**< Receive buffer full */
#define MFP_INT_TIMER_A    13   /**< Timer A */
#define MFP_INT_GPIO6      14   /**< I6 - RS-232 RI */
#define MFP_INT_GPIO7      15   /**< I7 - Monochrome detect */

/*===========================================================================*/
/* Timer Prescaler Dividers                                                  */
/*===========================================================================*/

/** Timer prescaler values (control register bits 0-2 for A/B, 0-2/4-6 for C/D) */
static const int mfp_timer_prescale[] = {
    0,      /* 0: Timer stopped */
    4,      /* 1: /4 */
    10,     /* 2: /10 */
    16,     /* 3: /16 */
    50,     /* 4: /50 */
    64,     /* 5: /64 */
    100,    /* 6: /100 */
    200     /* 7: /200 */
};

/*===========================================================================*/
/* Timer State                                                               */
/*===========================================================================*/

typedef struct {
    uint8_t  control;    /**< Timer control register value */
    uint8_t  data;       /**< Timer data register (counter reload) */
    uint16_t counter;    /**< Current counter value (0 means not loaded) */
    int      prescale;   /**< Current prescaler divisor */
    int      accum;      /**< Cycle accumulator for prescaling */
} mfp_timer_t;

/*===========================================================================*/
/* MFP State                                                                 */
/*===========================================================================*/

typedef struct {
    /* GPIO */
    uint8_t  gpip;       /**< GPIO pin states */
    uint8_t  aer;        /**< Active edge register */
    uint8_t  ddr;        /**< Data direction register */
    
    /* Interrupt Controller */
    uint8_t  iera;       /**< Interrupt enable A (sources 8-15) */
    uint8_t  ierb;       /**< Interrupt enable B (sources 0-7) */
    uint8_t  ipra;       /**< Interrupt pending A */
    uint8_t  iprb;       /**< Interrupt pending B */
    uint8_t  isra;       /**< Interrupt in-service A */
    uint8_t  isrb;       /**< Interrupt in-service B */
    uint8_t  imra;       /**< Interrupt mask A */
    uint8_t  imrb;       /**< Interrupt mask B */
    uint8_t  vr;         /**< Vector register (bits 7-4 = vector base) */
    
    /* Timers */
    mfp_timer_t timer_a;
    mfp_timer_t timer_b;
    mfp_timer_t timer_c;
    mfp_timer_t timer_d;
    
    /* USART */
    uint8_t  scr;        /**< Sync character */
    uint8_t  ucr;        /**< USART control */
    uint8_t  rsr;        /**< Receiver status */
    uint8_t  tsr;        /**< Transmitter status */
    uint8_t  udr;        /**< USART data */
    
    /* Internal state */
    bool     irq_out;    /**< Current IRQ output state */
} mfp_state_t;

/*===========================================================================*/
/* Internal Functions                                                        */
/*===========================================================================*/

/** Update timer prescaler from control register */
void mfp_timer_update_prescale(mfp_timer_t *timer);

/** Clock a single timer by the given number of MFP cycles */
bool mfp_timer_clock(mfp_timer_t *timer, int cycles);

/** Recalculate IRQ output state */
void mfp_update_irq(mfp_state_t *mfp);

/** Trigger an MFP interrupt */
void mfp_trigger_interrupt(mfp_state_t *mfp, int source);
