/**
 * @file st_glue.c
 * @brief Atari ST GLUE Chip Emulation Implementation
 * 
 * The GLUE manages video timing and interrupt priority.
 * It's the timing heart of the Atari ST.
 * 
 * Interrupt timing:
 *   HBL fires at the start of each visible scanline's horizontal blank
 *   VBL fires at scanline 263 (PAL) at the start of vertical blank
 *   MFP can fire at any time via its IRQ line
 * 
 * Priority arbitration:
 *   If VBL and HBL are both pending, VBL wins (level 4 > level 2)
 *   If MFP IRQ is active, it always wins (level 6)
 *   The 68000 only responds to interrupts above its current IPL mask
 * 
 * SPDX-License-Identifier: MIT
 */

#include <string.h>
#include "st_glue.h"

/*===========================================================================*/
/* Module State                                                              */
/*===========================================================================*/

static st_glue_t s_glue;

/*===========================================================================*/
/* Internal Functions                                                        */
/*===========================================================================*/

/**
 * @brief Recalculate and set the CPU IPL level based on pending interrupts
 * 
 * Priority (from IPL encoder):
 *   MFP active  -> IPL = 6
 *   VBL pending -> IPL = 4
 *   HBL pending -> IPL = 2
 *   None        -> IPL = 0
 */
static void update_ipl(void)
{
    int level = 0;
    
    /* HBL has lowest priority of the three */
    if (s_glue.hbl_pending) {
        level = 2;
    }
    
    /* VBL overrides HBL */
    if (s_glue.vbl_pending) {
        level = 4;
    }
    
    /* MFP has highest maskable priority */
    if (s_glue.mfp_irq) {
        level = 6;
    }
    
    if (level != s_glue.current_ipl) {
        s_glue.current_ipl = level;
        if (s_glue.set_irq) {
            s_glue.set_irq(level);
        }
    }
}

/**
 * @brief Process one scanline transition
 */
static void process_scanline_end(void)
{
    s_glue.scanline++;
    
    if (s_glue.scanline >= s_glue.lines_per_frame) {
        /* End of frame */
        s_glue.scanline = 0;
        s_glue.frame_count++;
    }
    
    /* Generate HBL interrupt for visible scanlines */
    if (s_glue.scanline >= ST_FIRST_VISIBLE_LINE &&
        s_glue.scanline < ST_VBL_LINE) {
        s_glue.hbl_pending = true;
    }
    
    /* Generate VBL interrupt */
    if (s_glue.scanline == ST_VBL_LINE) {
        s_glue.vbl_pending = true;
    }
}

/*===========================================================================*/
/* Public API                                                                */
/*===========================================================================*/

void st_glue_init(bool pal)
{
    memset(&s_glue, 0, sizeof(s_glue));
    
    s_glue.pal = pal;
    s_glue.lines_per_frame = pal ? 312 : 262;
}

void st_glue_reset(void)
{
    s_glue.scanline = 0;
    s_glue.line_cycles = 0;
    s_glue.frame_cycles = 0;
    s_glue.frame_count = 0;
    s_glue.hbl_pending = false;
    s_glue.vbl_pending = false;
    s_glue.mfp_irq = false;
    s_glue.current_ipl = 0;
    
    if (s_glue.set_irq) {
        s_glue.set_irq(0);
    }
}

void st_glue_connect_cpu(void (*set_irq)(int level))
{
    s_glue.set_irq = set_irq;
}

void st_glue_connect_mfp(void (*mfp_clock)(int cycles),
                          bool (*mfp_irq_pending)(void))
{
    s_glue.mfp_clock = mfp_clock;
    s_glue.mfp_irq_pending = mfp_irq_pending;
}

void st_glue_clock(int cpu_cycles)
{
    /* Convert CPU cycles to MFP cycles and clock the MFP.
     * CPU = 8 MHz, MFP = 2.4576 MHz
     * Ratio = 2.4576/8 = 0.3072
     * Use fixed-point: (cycles * 3072 + 5000) / 10000
     * Simplified: accumulate and convert */
    if (s_glue.mfp_clock) {
        /* Approximate: 3 MFP cycles per 10 CPU cycles */
        static int mfp_accum = 0;
        mfp_accum += cpu_cycles * 3072;
        int mfp_cycles = mfp_accum / 10000;
        mfp_accum -= mfp_cycles * 10000;
        
        if (mfp_cycles > 0) {
            s_glue.mfp_clock(mfp_cycles);
        }
    }
    
    /* Update MFP IRQ line state */
    if (s_glue.mfp_irq_pending) {
        s_glue.mfp_irq = s_glue.mfp_irq_pending();
    }
    
    /* Advance video timing */
    s_glue.line_cycles += cpu_cycles;
    s_glue.frame_cycles += cpu_cycles;
    
    /* Process scanline boundaries */
    while (s_glue.line_cycles >= ST_CYCLES_PER_LINE) {
        s_glue.line_cycles -= ST_CYCLES_PER_LINE;
        process_scanline_end();
    }
    
    /* Update interrupt priority */
    update_ipl();
    
    /* Clear HBL/VBL pending after the CPU has had a chance to see them.
     * On real hardware these are edge-triggered pulses.
     * We clear them after update_ipl has set the IPL level. 
     * The CPU will latch the level and process it. */
    s_glue.hbl_pending = false;
    s_glue.vbl_pending = false;
}

int st_glue_get_scanline(void)
{
    return s_glue.scanline;
}

uint32_t st_glue_get_frame_count(void)
{
    return s_glue.frame_count;
}

bool st_glue_in_visible_area(void)
{
    return (s_glue.scanline >= ST_FIRST_VISIBLE_LINE &&
            s_glue.scanline < ST_VBL_LINE);
}

st_glue_t* st_glue_get_state(void)
{
    return &s_glue;
}
