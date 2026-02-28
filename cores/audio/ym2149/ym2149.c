/**
 * @file ym2149.c
 * @brief YM2149 PSG Sound Chip - EBIN Component
 * 
 * Implements the YM-2149 Programmable Sound Generator as a loadable
 * EBIN component with audio_interface_t.
 * 
 * The YM PSG in the Atari ST runs at 2 MHz (8 MHz CPU clock / 4).
 * Tone frequencies: f = 2000000 / (16 * period)
 * Noise frequency:  f = 2000000 / (16 * period)
 * 
 * Register interface:
 *   $FF8800 read  = read currently selected register
 *   $FF8800 write = select register (0-15)
 *   $FF8802 write = write data to selected register
 * 
 * Amplitude levels follow a logarithmic scale. The DAC output
 * voltage levels from the AY-3-8910 datasheet approximate:
 *   Level 0 = 0.0V, Level 15 = max (~1.0V)
 * 
 * SPDX-License-Identifier: MIT
 */

#include <stdint.h>
#include <stdbool.h>
#include "ym2149_internal.h"

/* Freestanding memset — no libc in EBIN */
static inline void *ebin_memset(void *s, int c, unsigned int n)
{
    unsigned char *p = (unsigned char *)s;
    while (n--) *p++ = (unsigned char)c;
    return s;
}

/*===========================================================================*/
/* Global State                                                              */
/*===========================================================================*/

static ym2149_state_t g_ym;

/*===========================================================================*/
/* DAC Volume Table (logarithmic, 16-bit signed output)                      */
/*===========================================================================*/

/**
 * Logarithmic volume levels based on AY-3-8910 measurements.
 * Each step is approximately sqrt(2) (~3dB).
 */
static const int16_t ym_volume_table[16] = {
    0,     64,    94,    138,
    202,   296,   434,   636,
    932,   1366,  2002,  2934,
    4300,  6302,  9234,  13534
};

/*===========================================================================*/
/* Envelope Shapes                                                           */
/*===========================================================================*/

/**
 * Envelope shape patterns.
 * Each shape defines how the volume changes over 32 steps.
 * Bits 3-0 of register 13:
 *   Bit 3: Continue
 *   Bit 2: Attack
 *   Bit 1: Alternate
 *   Bit 0: Hold
 */
static uint8_t envelope_step_volume(uint8_t shape, uint8_t step)
{
    bool attack = shape & 0x04;
    
    /* First cycle (steps 0-15) */
    if (step < 16) {
        if (attack) {
            return step;         /* Ramp up */
        } else {
            return 15 - step;    /* Ramp down */
        }
    }
    
    /* Second cycle and beyond (steps 16-31) */
    if (!(shape & 0x08)) {
        /* Continue = 0: hold at 0 after first cycle */
        return 0;
    }
    
    if (shape & 0x01) {
        /* Hold: stay at final value of first cycle */
        if (attack) {
            return (shape & 0x02) ? 0 : 15;
        } else {
            return (shape & 0x02) ? 15 : 0;
        }
    }
    
    if (shape & 0x02) {
        /* Alternate */
        uint8_t s = step - 16;
        if (attack) {
            return 15 - s;       /* Ramp down */
        } else {
            return s;            /* Ramp up */
        }
    }
    
    /* Repeat first cycle pattern */
    uint8_t s = step - 16;
    if (attack) {
        return s;
    } else {
        return 15 - s;
    }
}

/*===========================================================================*/
/* Register Read/Write                                                       */
/*===========================================================================*/

static uint8_t ym_read_reg(uint32_t addr)
{
    /* $FF8800 read: return selected register value */
    (void)addr;  /* All reads from $FF8800 */
    
    uint8_t reg = g_ym.selected_reg;
    
    if (reg >= YM_NUM_REGS) return 0xFF;
    
    if (reg == YM_REG_PORT_A) {
        if (g_ym.port_a_read) {
            return g_ym.port_a_read(g_ym.port_a_ctx);
        }
        return g_ym.regs[YM_REG_PORT_A];
    }
    
    if (reg == YM_REG_PORT_B) {
        if (g_ym.port_b_read) {
            return g_ym.port_b_read(g_ym.port_b_ctx);
        }
        return g_ym.regs[YM_REG_PORT_B];
    }
    
    return g_ym.regs[reg];
}

static void ym_write_reg(uint32_t addr, uint8_t val)
{
    /* Determine if this is a register select or data write */
    uint32_t offset = addr & 0x03;
    
    if (offset == 0) {
        /* $FF8800 write: select register */
        g_ym.selected_reg = val & 0x0F;
        return;
    }
    
    /* $FF8802 write: write data to selected register */
    uint8_t reg = g_ym.selected_reg;
    if (reg >= YM_NUM_REGS) return;
    
    /* Mask register values to valid bits */
    switch (reg) {
        case YM_REG_FREQ_A_HI:
        case YM_REG_FREQ_B_HI:
        case YM_REG_FREQ_C_HI:
            val &= 0x0F;  /* 4-bit */
            break;
        case YM_REG_FREQ_NOISE:
            val &= 0x1F;  /* 5-bit */
            break;
        case YM_REG_AMP_A:
        case YM_REG_AMP_B:
        case YM_REG_AMP_C:
            val &= 0x1F;  /* 5-bit (bit 4 = envelope mode) */
            break;
        case YM_REG_ENV_SHAPE:
            val &= 0x0F;  /* 4-bit */
            /* Writing shape resets envelope */
            g_ym.envelope.step = 0;
            g_ym.envelope.counter = 0;
            g_ym.envelope.holding = false;
            break;
    }
    
    g_ym.regs[reg] = val;
    
    /* Update internal state based on register written */
    switch (reg) {
        case YM_REG_FREQ_A_LO:
        case YM_REG_FREQ_A_HI:
            g_ym.channel[0].period = g_ym.regs[YM_REG_FREQ_A_LO] |
                                     ((uint16_t)(g_ym.regs[YM_REG_FREQ_A_HI]) << 8);
            break;
        case YM_REG_FREQ_B_LO:
        case YM_REG_FREQ_B_HI:
            g_ym.channel[1].period = g_ym.regs[YM_REG_FREQ_B_LO] |
                                     ((uint16_t)(g_ym.regs[YM_REG_FREQ_B_HI]) << 8);
            break;
        case YM_REG_FREQ_C_LO:
        case YM_REG_FREQ_C_HI:
            g_ym.channel[2].period = g_ym.regs[YM_REG_FREQ_C_LO] |
                                     ((uint16_t)(g_ym.regs[YM_REG_FREQ_C_HI]) << 8);
            break;
        case YM_REG_FREQ_NOISE:
            g_ym.noise_period = val;
            break;
        case YM_REG_ENV_LO:
        case YM_REG_ENV_HI:
            g_ym.envelope.period = g_ym.regs[YM_REG_ENV_LO] |
                                   ((uint16_t)(g_ym.regs[YM_REG_ENV_HI]) << 8);
            break;
        case YM_REG_ENV_SHAPE:
            g_ym.envelope.shape = val;
            break;
        case YM_REG_PORT_A:
            if (g_ym.port_a_write) {
                g_ym.port_a_write(val, g_ym.port_a_ctx);
            }
            break;
        case YM_REG_PORT_B:
            if (g_ym.port_b_write) {
                g_ym.port_b_write(val, g_ym.port_b_ctx);
            }
            break;
    }
}

/*===========================================================================*/
/* Sound Generation                                                          */
/*===========================================================================*/

/**
 * @brief Clock the tone generators and noise for one PSG cycle
 * 
 * The PSG internally divides by 8, so one "step" = 8 clock cycles.
 * At 2 MHz input, one step = 4 microseconds.
 */
static void ym_clock_step(void)
{
    /* Clock tone channels */
    for (int ch = 0; ch < 3; ch++) {
        if (g_ym.channel[ch].period > 0) {
            g_ym.channel[ch].counter++;
            if (g_ym.channel[ch].counter >= g_ym.channel[ch].period) {
                g_ym.channel[ch].counter = 0;
                g_ym.channel[ch].output ^= 1;
            }
        }
    }
    
    /* Clock noise generator */
    if (g_ym.noise_period > 0) {
        g_ym.noise_counter++;
        if (g_ym.noise_counter >= g_ym.noise_period) {
            g_ym.noise_counter = 0;
            /* 17-bit LFSR: x^17 + x^14 + 1 */
            uint32_t bit = ((g_ym.noise_rng >> 0) ^ (g_ym.noise_rng >> 3)) & 1;
            g_ym.noise_rng = (g_ym.noise_rng >> 1) | (bit << 16);
            g_ym.noise_output = g_ym.noise_rng & 1;
        }
    }
    
    /* Clock envelope */
    if (!g_ym.envelope.holding && g_ym.envelope.period > 0) {
        g_ym.envelope.counter++;
        if (g_ym.envelope.counter >= g_ym.envelope.period) {
            g_ym.envelope.counter = 0;
            g_ym.envelope.step++;
            if (g_ym.envelope.step >= 32) {
                /* Check if envelope continues or holds */
                if (g_ym.envelope.shape & 0x01) {
                    g_ym.envelope.holding = true;
                    g_ym.envelope.step = 31;
                } else if (g_ym.envelope.shape & 0x08) {
                    g_ym.envelope.step = 16;  /* Repeat from second half */
                } else {
                    g_ym.envelope.step = 31;
                    g_ym.envelope.holding = true;
                }
            }
            g_ym.envelope.volume = envelope_step_volume(
                g_ym.envelope.shape, g_ym.envelope.step);
        }
    }
}

/**
 * @brief Get the output sample for one channel
 */
static int16_t ym_channel_output(int ch)
{
    uint8_t mixer = g_ym.regs[YM_REG_MIXER];
    
    /* Tone enable (active low) */
    bool tone_on = !(mixer & (1 << ch));
    /* Noise enable (active low) */
    bool noise_on = !(mixer & (1 << (ch + 3)));
    
    /* Output is OR of enabled sources (tone, noise) */
    uint8_t out = 1;
    if (tone_on) {
        out &= g_ym.channel[ch].output;
    }
    if (noise_on) {
        out &= g_ym.noise_output;
    }
    if (!tone_on && !noise_on) {
        out = 1;  /* Both disabled = always on */
    }
    
    /* Get volume */
    uint8_t amp_reg = g_ym.regs[YM_REG_AMP_A + ch];
    int16_t volume;
    if (amp_reg & 0x10) {
        /* Envelope mode */
        volume = ym_volume_table[g_ym.envelope.volume];
    } else {
        volume = ym_volume_table[amp_reg & 0x0F];
    }
    
    return out ? volume : 0;
}

/*===========================================================================*/
/* audio_interface_t Implementation                                          */
/*===========================================================================*/

typedef struct {
    uint32_t sample_rate;
    void    *context;
} simple_audio_config_t;

static int ym_init(simple_audio_config_t *config)
{
    (void)config;
    ebin_memset(&g_ym, 0, sizeof(g_ym));
    
    /* Initialize LFSR to non-zero */
    g_ym.noise_rng = 1;
    
    /* Default port A: color monitor, no drives selected */
    g_ym.regs[YM_REG_PORT_A] = YM_PA_MONO | YM_PA_DRIVE_A | YM_PA_DRIVE_B;
    g_ym.regs[YM_REG_MIXER] = 0xFF;  /* All disabled */
    
    return 0;
}

static void ym_reset(void)
{
    uint8_t saved_porta = g_ym.regs[YM_REG_PORT_A];
    
    /* Clear all registers */
    ebin_memset(g_ym.regs, 0, sizeof(g_ym.regs));
    
    /* Reset all channels */
    ebin_memset(g_ym.channel, 0, sizeof(g_ym.channel));
    g_ym.noise_rng = 1;
    g_ym.noise_counter = 0;
    g_ym.noise_output = 0;
    g_ym.selected_reg = 0;
    
    /* Preserve port A hardware state */
    g_ym.regs[YM_REG_PORT_A] = saved_porta;
    g_ym.regs[YM_REG_MIXER] = 0xFF;
    
    ebin_memset(&g_ym.envelope, 0, sizeof(g_ym.envelope));
}

static void ym_shutdown(void)
{
    ebin_memset(&g_ym, 0, sizeof(g_ym));
}

/**
 * @brief Generate audio samples
 * 
 * Generates stereo interleaved signed 16-bit PCM samples.
 * The ST outputs mono through the PSG (both channels get same data).
 * 
 * We need to generate PSG clock ticks proportional to the output
 * sample rate. PSG clock = 2 MHz, internal divider = 8, so
 * effective tone clock = 250 kHz.
 */
static void ym_generate(int16_t *buffer, int samples)
{
    /* For now, generate samples at output rate.
     * PSG effective clock = 250 kHz (2MHz / 8)
     * At 48 kHz output: ~5.2 PSG steps per sample */
    
    for (int i = 0; i < samples; i++) {
        /* Advance PSG state */
        ym_clock_step();
        
        /* Mix three channels */
        int32_t mix = 0;
        mix += ym_channel_output(0);
        mix += ym_channel_output(1);
        mix += ym_channel_output(2);
        
        /* Divide by 3 and clamp */
        int16_t sample = (int16_t)(mix / 3);
        
        /* Stereo: same on both channels (ST PSG is mono) */
        buffer[i * 2]     = sample;
        buffer[i * 2 + 1] = sample;
    }
}

/**
 * @brief Clock the PSG by CPU cycles
 * 
 * Converts CPU cycles to PSG internal clock cycles.
 * CPU = 8 MHz, PSG clock = 2 MHz, internal divider = 8
 * So: 1 PSG step = 32 CPU cycles
 */
static void ym_clock(int cycles)
{
    static int accum = 0;
    accum += cycles;
    
    while (accum >= 32) {
        accum -= 32;
        ym_clock_step();
    }
}

static void ym_set_bus(void *bus)
{
    /* PSG doesn't need bus access */
    (void)bus;
}

/*===========================================================================*/
/* Interface Structure                                                       */
/*===========================================================================*/

static const struct {
    uint32_t    interface_version;
    const char *name;
    
    int  (*init)(simple_audio_config_t *config);
    void (*reset)(void);
    void (*shutdown)(void);
    
    void (*generate)(int16_t *buffer, int samples);
    
    void (*clock)(int cycles);
    
    uint8_t (*read_reg)(uint32_t addr);
    void    (*write_reg)(uint32_t addr, uint8_t val);
    
    void (*set_bus)(void *bus);
} s_interface = {
    .interface_version = 0x00010000,  /* AUDIO_INTERFACE_V1 */
    .name              = "YM2149 PSG",
    
    .init       = ym_init,
    .reset      = ym_reset,
    .shutdown   = ym_shutdown,
    
    .generate   = ym_generate,
    
    .clock      = ym_clock,
    
    .read_reg   = ym_read_reg,
    .write_reg  = ym_write_reg,
    
    .set_bus    = ym_set_bus,
};

/*===========================================================================*/
/* EBIN Entry Point                                                          */
/*===========================================================================*/

void* component_entry(void)
{
    return (void*)&s_interface;
}
