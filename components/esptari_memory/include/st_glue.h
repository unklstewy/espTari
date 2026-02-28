/**
 * @file st_glue.h
 * @brief Atari ST GLUE Chip Emulation
 * 
 * The GLUE (Generalized Logic Unit) is the Atari ST's custom chip that
 * handles address decoding, interrupt priority, and system timing.
 * 
 * Key Functions:
 *   - Generate HBL (Horizontal Blank) interrupt at IPL level 2
 *   - Generate VBL (Vertical Blank) interrupt at IPL level 4
 *   - Route MFP interrupt to IPL level 6
 *   - Bus arbitration between CPU and DMA
 *   - Address decode chip-selects for ROM, RAM, I/O
 * 
 * Interrupt Priority (active-low IPL2:IPL1:IPL0):
 *   Level 1: Unused (active low)
 *   Level 2: HBL (Horizontal Blank)
 *   Level 3: Unused
 *   Level 4: VBL (Vertical Blank) 
 *   Level 5: Unused
 *   Level 6: MFP 68901
 *   Level 7: NMI (active - active low on IPL lines)
 * 
 * Video Timing (PAL, 50Hz):
 *   312 scanlines per frame, 160256 cycles per frame
 *   512 cycles per scanline
 *   47-48 HBL interrupts per frame (visible portion)
 * 
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "component_api.h"

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================*/
/* Video Timing Constants (PAL 50Hz)                                         */
/*===========================================================================*/

/** CPU clock frequency (Hz) */
#define ST_CPU_CLOCK_HZ         8000000

/** MFP clock frequency (Hz) */
#define ST_MFP_CLOCK_HZ        2457600

/** CPU cycles per scanline (8MHz / ~15.625kHz) */
#define ST_CYCLES_PER_LINE      512

/** Total scanlines per frame (PAL) */
#define ST_LINES_PER_FRAME      312

/** Visible scanlines (PAL) */
#define ST_VISIBLE_LINES        200

/** First visible scanline (PAL, 50Hz) */
#define ST_FIRST_VISIBLE_LINE   63

/** VBL scanline (after last visible line) */
#define ST_VBL_LINE             (ST_FIRST_VISIBLE_LINE + ST_VISIBLE_LINES)

/** Total cycles per frame */
#define ST_CYCLES_PER_FRAME     (ST_CYCLES_PER_LINE * ST_LINES_PER_FRAME)

/** Frames per second (PAL) */
#define ST_FPS_PAL              50

/** Frames per second (NTSC) */
#define ST_FPS_NTSC             60

/*===========================================================================*/
/* GLUE State                                                                */
/*===========================================================================*/

typedef struct {
    /* Video timing counters */
    int      scanline;       /**< Current scanline (0-311 PAL) */
    int      line_cycles;    /**< Cycles within current scanline */
    int      frame_cycles;   /**< Total cycles this frame */
    uint32_t frame_count;    /**< Frame counter */
    
    /* Interrupt state */
    bool     hbl_pending;    /**< HBL interrupt pending */
    bool     vbl_pending;    /**< VBL interrupt pending */
    bool     mfp_irq;       /**< MFP IRQ line state */
    int      current_ipl;    /**< Current IPL level output to CPU */
    
    /* CPU interface for setting IRQ level */
    void (*set_irq)(int level);
    
    /* MFP interface for clocking */
    void (*mfp_clock)(int cycles);
    bool (*mfp_irq_pending)(void);
    
    /* Video timing */
    bool     pal;            /**< true = PAL (50Hz), false = NTSC (60Hz) */
    int      lines_per_frame;
    
} st_glue_t;

/*===========================================================================*/
/* Public API                                                                */
/*===========================================================================*/

/**
 * @brief Initialize the GLUE chip
 * 
 * @param[in] pal  true for PAL (50Hz), false for NTSC (60Hz)
 */
void st_glue_init(bool pal);

/**
 * @brief Reset GLUE state
 */
void st_glue_reset(void);

/**
 * @brief Connect CPU interrupt function
 */
void st_glue_connect_cpu(void (*set_irq)(int level));

/**
 * @brief Connect MFP clock and IRQ functions
 */
void st_glue_connect_mfp(void (*mfp_clock)(int cycles),
                          bool (*mfp_irq_pending)(void));

/**
 * @brief Advance the GLUE by CPU cycles
 * 
 * This is the main timing function. It:
 * - Advances the video timing counters
 * - Generates HBL/VBL interrupts at the correct times
 * - Clocks the MFP
 * - Updates the CPU IPL lines
 * 
 * @param[in] cpu_cycles Number of CPU cycles to advance
 */
void st_glue_clock(int cpu_cycles);

/**
 * @brief Get current scanline
 */
int st_glue_get_scanline(void);

/**
 * @brief Get current frame count
 */
uint32_t st_glue_get_frame_count(void);

/**
 * @brief Check if we're in the visible area
 */
bool st_glue_in_visible_area(void);

/**
 * @brief Get the GLUE state (for save/restore)
 */
st_glue_t* st_glue_get_state(void);

#ifdef __cplusplus
}
#endif
