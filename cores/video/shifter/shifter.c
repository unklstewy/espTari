/**
 * @file shifter.c
 * @brief Atari ST Shifter Video Controller - EBIN Component
 * 
 * Implements the ST Shifter video chip as a loadable EBIN
 * component with video_interface_t.
 * 
 * Phase 2 scope:
 *   - Register read/write for all Shifter registers
 *   - Video base address and counter management
 *   - Resolution and palette registers
 *   - Basic scanline rendering (planar to RGB565)
 * 
 * The Shifter reads video RAM via DMA from the address in the
 * video base register. On real hardware, the DMA steals bus cycles
 * from the CPU during the visible portion of each scanline.
 * 
 * ST Color Format: ----rrr-ggg-bbb-  (3 bits per component, bit 0 unused)
 * STe Color Format: ----RrrrGgggBbbb (4 bits per component)
 * 
 * SPDX-License-Identifier: MIT
 */

#include <stdint.h>
#include <stdbool.h>
#include "shifter_internal.h"

/* No libc in EBIN - provide our own */
static void *ebin_memset(void *s, int c, unsigned int n)
{
    unsigned char *p = (unsigned char *)s;
    while (n--) *p++ = (unsigned char)c;
    return s;
}

/*===========================================================================*/
/* Global State                                                              */
/*===========================================================================*/

static shifter_state_t g_shifter;

/*===========================================================================*/
/* Palette Conversion                                                        */
/*===========================================================================*/

/**
 * @brief Convert ST palette entry to RGB565
 * 
 * ST format: ----rrr-ggg-bbb-
 *   R = bits 10-8 (3 bits)
 *   G = bits  6-4 (3 bits)
 *   B = bits  2-0 (3 bits, bit 0 unused on ST, used on STe)
 */
uint16_t shifter_st_to_rgb565(uint16_t st_color)
{
    /* Extract 3-bit components (ST) */
    uint8_t r3 = (st_color >> 8) & 0x07;
    uint8_t g3 = (st_color >> 4) & 0x07;
    uint8_t b3 = (st_color >> 0) & 0x07;
    
    /* Scale 3-bit (0-7) to RGB565 ranges */
    /* R: 3-bit -> 5-bit: val * 31 / 7 ≈ val * 4 + val/2 */
    uint8_t r5 = (r3 << 2) | (r3 >> 1);
    /* G: 3-bit -> 6-bit: val * 63 / 7 ≈ val * 9 */
    uint8_t g6 = (g3 << 3) | g3;
    /* B: 3-bit -> 5-bit */
    uint8_t b5 = (b3 << 2) | (b3 >> 1);
    
    return (r5 << 11) | (g6 << 5) | b5;
}

void shifter_update_palette(shifter_state_t *s)
{
    for (int i = 0; i < 16; i++) {
        s->palette_rgb565[i] = shifter_st_to_rgb565(s->palette[i]);
    }
}

/*===========================================================================*/
/* Register Access                                                           */
/*===========================================================================*/

static uint8_t shifter_read_byte(uint32_t addr)
{
    uint32_t reg = addr - 0xFF8200;
    
    switch (reg) {
        case SHIFT_REG_BASE_HI:
            return (uint8_t)(g_shifter.video_base >> 16) & 0x3F;
        case SHIFT_REG_BASE_MID:
            return (uint8_t)(g_shifter.video_base >> 8);
        case SHIFT_REG_BASE_LO:
            return (uint8_t)(g_shifter.video_base);
            
        case SHIFT_REG_COUNT_HI:
            return (uint8_t)(g_shifter.video_counter >> 16) & 0x3F;
        case SHIFT_REG_COUNT_MID:
            return (uint8_t)(g_shifter.video_counter >> 8);
        case SHIFT_REG_COUNT_LO:
            return (uint8_t)(g_shifter.video_counter);
            
        case SHIFT_REG_SYNC:
            return g_shifter.sync_mode;
            
        case SHIFT_REG_RES:
            return g_shifter.resolution;
            
        case SHIFT_REG_LINEWIDTH:
            return g_shifter.line_width;
            
        default:
            /* Palette registers are word-addressed */
            if (reg >= SHIFT_REG_PALETTE && reg <= SHIFT_REG_PALETTE_END) {
                int idx = (reg - SHIFT_REG_PALETTE) / 2;
                if (idx < 16) {
                    if (reg & 1) {
                        return (uint8_t)(g_shifter.palette[idx] & 0xFF);
                    } else {
                        return (uint8_t)(g_shifter.palette[idx] >> 8);
                    }
                }
            }
            return 0xFF;
    }
}

static uint16_t shifter_read_word(uint32_t addr)
{
    uint32_t reg = addr - 0xFF8200;
    
    /* Palette reads are naturally word-sized */
    if (reg >= SHIFT_REG_PALETTE && reg <= SHIFT_REG_PALETTE_END) {
        int idx = (reg - SHIFT_REG_PALETTE) / 2;
        if (idx < 16) {
            return g_shifter.palette[idx];
        }
    }
    
    /* Other registers: compose from byte reads */
    return ((uint16_t)shifter_read_byte(addr) << 8) | 
           shifter_read_byte(addr + 1);
}

static void shifter_write_byte(uint32_t addr, uint8_t val)
{
    uint32_t reg = addr - 0xFF8200;
    
    switch (reg) {
        case SHIFT_REG_BASE_HI:
            g_shifter.video_base = (g_shifter.video_base & 0x00FFFF) |
                                   ((uint32_t)(val & 0x3F) << 16);
            break;
        case SHIFT_REG_BASE_MID:
            g_shifter.video_base = (g_shifter.video_base & 0xFF00FF) |
                                   ((uint32_t)val << 8);
            break;
        case SHIFT_REG_BASE_LO:
            /* Low byte writable on STe only, must be even */
            g_shifter.video_base = (g_shifter.video_base & 0xFFFF00) |
                                   (val & 0xFE);
            break;
            
        case SHIFT_REG_SYNC:
            g_shifter.sync_mode = val & 0x03;
            break;
            
        case SHIFT_REG_RES:
            g_shifter.resolution = val & 0x03;
            break;
            
        case SHIFT_REG_LINEWIDTH:
            g_shifter.line_width = val;
            break;
            
        default:
            /* Palette registers - handled in word write */
            break;
    }
}

static void shifter_write_word(uint32_t addr, uint16_t val)
{
    uint32_t reg = addr - 0xFF8200;
    
    /* Palette writes */
    if (reg >= SHIFT_REG_PALETTE && reg <= SHIFT_REG_PALETTE_END) {
        int idx = (reg - SHIFT_REG_PALETTE) / 2;
        if (idx < 16) {
            /* Mask to valid ST bits: ----rrr-ggg-bbb- */
            g_shifter.palette[idx] = val & 0x0FFF;
            g_shifter.palette_rgb565[idx] = shifter_st_to_rgb565(val & 0x0FFF);
        }
        return;
    }
    
    /* Other registers: split into byte writes */
    shifter_write_byte(addr, (uint8_t)(val >> 8));
    shifter_write_byte(addr + 1, (uint8_t)(val & 0xFF));
}

/*===========================================================================*/
/* video_interface_t Implementation                                          */
/*===========================================================================*/

typedef struct {
    void    *framebuffer;
    uint32_t fb_size;
    void    *context;
} simple_video_config_t;

static int shifter_init(simple_video_config_t *config)
{
    (void)config;
    ebin_memset(&g_shifter, 0, sizeof(g_shifter));
    
    /* Default palette: TOS default colors */
    g_shifter.palette[0] = 0x0FFF;  /* White */
    g_shifter.palette[1] = 0x0F00;  /* Red */
    g_shifter.palette[2] = 0x00F0;  /* Green */
    g_shifter.palette[3] = 0x0000;  /* Black */
    /* Rest default to black */
    
    shifter_update_palette(&g_shifter);
    
    return 0;  /* ESP_OK */
}

static void shifter_reset(void)
{
    g_shifter.video_base = 0;
    g_shifter.video_counter = 0;
    g_shifter.resolution = ST_RES_LOW;
    g_shifter.sync_mode = 0;
    g_shifter.line_width = 0;
    
    /* Reset palette to TOS defaults */
    ebin_memset(g_shifter.palette, 0, sizeof(g_shifter.palette));
    g_shifter.palette[0] = 0x0FFF;
    g_shifter.palette[1] = 0x0F00;
    g_shifter.palette[2] = 0x00F0;
    shifter_update_palette(&g_shifter);
}

static void shifter_shutdown(void)
{
    ebin_memset(&g_shifter, 0, sizeof(g_shifter));
}

/**
 * @brief Render one scanline in low resolution (320x200, 16 colors)
 * 
 * Low res uses 4 bitplanes. Each group of 4 words (8 bytes) produces
 * 16 pixels. A full line is 20 groups = 80 bytes = 160 words.
 */
static void render_scanline_low(int line, uint16_t *buffer)
{
    if (!g_shifter.bus_read_word) {
        ebin_memset(buffer, 0, 320 * sizeof(uint16_t));
        return;
    }
    
    uint32_t addr = g_shifter.video_base + (line * 160);
    
    for (int group = 0; group < 20; group++) {
        /* Read 4 bitplane words */
        uint16_t plane0 = g_shifter.bus_read_word(addr);
        uint16_t plane1 = g_shifter.bus_read_word(addr + 2);
        uint16_t plane2 = g_shifter.bus_read_word(addr + 4);
        uint16_t plane3 = g_shifter.bus_read_word(addr + 6);
        addr += 8;
        
        /* Convert 16 pixels */
        for (int bit = 15; bit >= 0; bit--) {
            int color = ((plane0 >> bit) & 1) |
                       (((plane1 >> bit) & 1) << 1) |
                       (((plane2 >> bit) & 1) << 2) |
                       (((plane3 >> bit) & 1) << 3);
            *buffer++ = g_shifter.palette_rgb565[color];
        }
    }
}

/**
 * @brief Render one scanline in medium resolution (640x200, 4 colors)
 */
static void render_scanline_med(int line, uint16_t *buffer)
{
    if (!g_shifter.bus_read_word) {
        ebin_memset(buffer, 0, 640 * sizeof(uint16_t));
        return;
    }
    
    uint32_t addr = g_shifter.video_base + (line * 160);
    
    for (int group = 0; group < 40; group++) {
        uint16_t plane0 = g_shifter.bus_read_word(addr);
        uint16_t plane1 = g_shifter.bus_read_word(addr + 2);
        addr += 4;
        
        for (int bit = 15; bit >= 0; bit--) {
            int color = ((plane0 >> bit) & 1) |
                       (((plane1 >> bit) & 1) << 1);
            *buffer++ = g_shifter.palette_rgb565[color];
        }
    }
}

/**
 * @brief Render one scanline in high resolution (640x400, monochrome)
 */
static void render_scanline_high(int line, uint16_t *buffer)
{
    if (!g_shifter.bus_read_word) {
        ebin_memset(buffer, 0, 640 * sizeof(uint16_t));
        return;
    }
    
    uint32_t addr = g_shifter.video_base + (line * 80);
    
    for (int word = 0; word < 40; word++) {
        uint16_t plane0 = g_shifter.bus_read_word(addr);
        addr += 2;
        
        for (int bit = 15; bit >= 0; bit--) {
            /* Mono: 0 = white (palette[0]), 1 = black (palette[1]) */
            int color = (plane0 >> bit) & 1;
            *buffer++ = g_shifter.palette_rgb565[color];
        }
    }
}

static void shifter_render_scanline(int line, uint16_t *buffer)
{
    switch (g_shifter.resolution) {
        case ST_RES_LOW:
            render_scanline_low(line, buffer);
            break;
        case ST_RES_MED:
            render_scanline_med(line, buffer);
            break;
        case ST_RES_HIGH:
            render_scanline_high(line, buffer);
            break;
        default:
            break;
    }
}

static void shifter_render_frame(void *framebuffer)
{
    uint16_t *fb = (uint16_t *)framebuffer;
    int height = (g_shifter.resolution == ST_RES_HIGH) ? 400 : 200;
    int width = (g_shifter.resolution == ST_RES_LOW) ? 320 : 640;
    
    for (int line = 0; line < height; line++) {
        shifter_render_scanline(line, fb + line * width);
    }
}

static int shifter_get_hpos(void)
{
    /* Simplified - would need GLUE timing info */
    return 0;
}

static int shifter_get_vpos(void)
{
    return 0;
}

static bool shifter_in_vblank(void)
{
    return false;
}

static bool shifter_in_hblank(void)
{
    return false;
}

static void shifter_clock(int cycles)
{
    /* Video counter advances during visible display.
     * For now, this is handled by render_scanline reading from video_base.
     * In a more accurate implementation, this would update video_counter. */
    (void)cycles;
}

static uint16_t shifter_io_read_reg(uint32_t addr)
{
    return shifter_read_word(addr);
}

static void shifter_io_write_reg(uint32_t addr, uint16_t val)
{
    shifter_write_word(addr, val);
}

static void shifter_get_mode(void *mode_ptr)
{
    /* Manual mode struct to avoid including component_api.h types */
    struct {
        uint16_t width;
        uint16_t height;
        uint8_t  bpp;
        uint8_t  interlaced;
        uint32_t pixel_clock;
        uint16_t h_total;
        uint16_t v_total;
    } *mode = mode_ptr;
    
    switch (g_shifter.resolution) {
        case ST_RES_LOW:
            mode->width = 320;
            mode->height = 200;
            mode->bpp = 4;
            break;
        case ST_RES_MED:
            mode->width = 640;
            mode->height = 200;
            mode->bpp = 2;
            break;
        case ST_RES_HIGH:
            mode->width = 640;
            mode->height = 400;
            mode->bpp = 1;
            break;
        default:
            mode->width = 320;
            mode->height = 200;
            mode->bpp = 4;
            break;
    }
    mode->interlaced = 0;
    mode->pixel_clock = 8000000;  /* 8 MHz */
    mode->h_total = 512;
    mode->v_total = 312;
}

static void shifter_set_bus(void *bus)
{
    if (bus) {
        void **ptrs = (void **)bus;
        g_shifter.bus_read_byte = (uint8_t (*)(uint32_t))ptrs[0];
        g_shifter.bus_read_word = (uint16_t (*)(uint32_t))ptrs[1];
        g_shifter.bus_context = ptrs[8];
    }
}

/*===========================================================================*/
/* Interface Structure                                                       */
/*===========================================================================*/

static const struct {
    uint32_t    interface_version;
    const char *name;
    
    int  (*init)(simple_video_config_t *config);
    void (*reset)(void);
    void (*shutdown)(void);
    
    void (*render_scanline)(int line, uint16_t *buffer);
    void (*render_frame)(void *framebuffer);
    
    int  (*get_hpos)(void);
    int  (*get_vpos)(void);
    bool (*in_vblank)(void);
    bool (*in_hblank)(void);
    void (*clock)(int cycles);
    
    uint16_t (*read_reg)(uint32_t addr);
    void     (*write_reg)(uint32_t addr, uint16_t val);
    
    void (*get_mode)(void *mode);
    
    void (*set_bus)(void *bus);
} s_interface = {
    .interface_version = 0x00010000,  /* VIDEO_INTERFACE_V1 */
    .name              = "Shifter ST",
    
    .init            = shifter_init,
    .reset           = shifter_reset,
    .shutdown        = shifter_shutdown,
    
    .render_scanline = shifter_render_scanline,
    .render_frame    = shifter_render_frame,
    
    .get_hpos        = shifter_get_hpos,
    .get_vpos        = shifter_get_vpos,
    .in_vblank       = shifter_in_vblank,
    .in_hblank       = shifter_in_hblank,
    .clock           = shifter_clock,
    
    .read_reg        = shifter_io_read_reg,
    .write_reg       = shifter_io_write_reg,
    
    .get_mode        = shifter_get_mode,
    
    .set_bus         = shifter_set_bus,
};

/*===========================================================================*/
/* EBIN Entry Point                                                          */
/*===========================================================================*/

void* shifter_entry(void)
{
    return (void*)&s_interface;
}
