/**
 * @file component_api.h
 * @brief Component interface definitions for dynamically loaded emulation components
 * 
 * Each component type (CPU, video, audio, I/O) has a standardized interface
 * that allows the loader to interact with it uniformly. Components implement
 * these interfaces and export them via their entry point.
 * 
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================*/
/* Interface Versions                                                        */
/*===========================================================================*/

/** CPU interface version 1.0 */
#define CPU_INTERFACE_V1    0x00010000

/** Video interface version 1.0 */
#define VIDEO_INTERFACE_V1  0x00010000

/** Audio interface version 1.0 */
#define AUDIO_INTERFACE_V1  0x00010000

/** I/O interface version 1.0 */
#define IO_INTERFACE_V1     0x00010000

/** Unified monolithic system interface version 1.0 */
#define SYSTEM_INTERFACE_V1 0x00010000

/*===========================================================================*/
/* Forward Declarations                                                      */
/*===========================================================================*/

typedef struct bus_interface_s bus_interface_t;

/*===========================================================================*/
/* CPU State Structure                                                       */
/*===========================================================================*/

/**
 * @brief Generic 68K CPU state (common across 68000-68060)
 */
typedef struct {
    uint32_t d[8];          /**< Data registers D0-D7 */
    uint32_t a[8];          /**< Address registers A0-A7 */
    uint32_t pc;            /**< Program counter */
    uint32_t usp;           /**< User stack pointer */
    uint32_t ssp;           /**< Supervisor stack pointer */
    uint32_t msp;           /**< Master stack pointer (68020+) */
    uint32_t isp;           /**< Interrupt stack pointer (68020+) */
    uint16_t sr;            /**< Status register */
    uint32_t vbr;           /**< Vector base register (68010+) */
    uint32_t cacr;          /**< Cache control register (68020+) */
    uint32_t caar;          /**< Cache address register (68020+) */
    uint32_t sfc;           /**< Source function code (68010+) */
    uint32_t dfc;           /**< Destination function code (68010+) */
    uint8_t  stopped;       /**< STOP instruction active */
    uint8_t  halted;        /**< HALT state */
    int      pending_irq;   /**< Pending interrupt level (0-7) */
    uint64_t cycles;        /**< Total cycles executed */
} cpu_state_t;

/**
 * @brief CPU feature flags
 */
#define CPU_FEATURE_FPU         (1 << 0)   /**< Has FPU */
#define CPU_FEATURE_MMU         (1 << 1)   /**< Has MMU */
#define CPU_FEATURE_CACHE       (1 << 2)   /**< Has cache */
#define CPU_FEATURE_BURST       (1 << 3)   /**< Burst mode capable */
#define CPU_FEATURE_PIPELINE    (1 << 4)   /**< Pipelined execution */

/*===========================================================================*/
/* Bus Interface (provided to components)                                    */
/*===========================================================================*/

/**
 * @brief Bus interface provided by system to components
 * 
 * Components use this to access memory and I/O.
 */
struct bus_interface_s {
    /** Read byte from address */
    uint8_t  (*read_byte)(uint32_t addr);
    /** Read word from address (must be aligned) */
    uint16_t (*read_word)(uint32_t addr);
    /** Read long from address (must be aligned) */
    uint32_t (*read_long)(uint32_t addr);
    /** Write byte to address */
    void     (*write_byte)(uint32_t addr, uint8_t val);
    /** Write word to address (must be aligned) */
    void     (*write_word)(uint32_t addr, uint16_t val);
    /** Write long to address (must be aligned) */
    void     (*write_long)(uint32_t addr, uint32_t val);
    /** Signal bus error */
    void     (*bus_error)(uint32_t addr, bool write);
    /** Signal address error */
    void     (*address_error)(uint32_t addr, bool write);
    /** User context pointer */
    void     *context;
};

/*===========================================================================*/
/* CPU Interface                                                             */
/*===========================================================================*/

/**
 * @brief CPU component configuration
 */
typedef struct {
    uint32_t clock_hz;      /**< CPU clock frequency */
    void    *context;       /**< User context */
} cpu_config_t;

/**
 * @brief CPU component interface
 */
typedef struct {
    /** Interface version (must be CPU_INTERFACE_V1) */
    uint32_t interface_version;
    
    /** CPU name (e.g., "MC68000", "MC68030") */
    const char *name;
    
    /** Feature flags (CPU_FEATURE_*) */
    uint32_t features;
    
    /*--- Lifecycle ---*/
    
    /** Initialize CPU with configuration */
    int  (*init)(cpu_config_t *config);
    
    /** Reset CPU to initial state */
    void (*reset)(void);
    
    /** Shutdown and cleanup */
    void (*shutdown)(void);
    
    /*--- Execution ---*/
    
    /** Execute up to 'cycles' CPU cycles, return actual cycles consumed */
    int  (*execute)(int cycles);
    
    /** Stop execution immediately */
    void (*stop)(void);
    
    /*--- State ---*/
    
    /** Get current CPU state */
    void (*get_state)(cpu_state_t *state);
    
    /** Set CPU state (for save/restore) */
    void (*set_state)(const cpu_state_t *state);
    
    /*--- Interrupts ---*/
    
    /** Set interrupt request level (0-7, 0 = no interrupt) */
    void (*set_irq)(int level);
    
    /** Trigger NMI (level 7 edge-triggered) */
    void (*set_nmi)(void);
    
    /*--- Bus ---*/
    
    /** Set bus interface for memory access */
    void (*set_bus)(bus_interface_t *bus);
    
    /*--- Debug (optional, may be NULL) ---*/
    
    /** Disassemble instruction at PC, return bytes consumed */
    int  (*disassemble)(uint32_t pc, char *buf, int buf_len);
    
    /** Set breakpoint at address */
    void (*set_breakpoint)(uint32_t addr);
    
    /** Clear breakpoint at address */
    void (*clear_breakpoint)(uint32_t addr);
    
    /** Single-step one instruction */
    int  (*step)(void);
    
} cpu_interface_t;

/*===========================================================================*/
/* Video Mode                                                                */
/*===========================================================================*/

/**
 * @brief Video mode information
 */
typedef struct {
    uint16_t width;         /**< Width in pixels */
    uint16_t height;        /**< Height in pixels */
    uint8_t  bpp;           /**< Bits per pixel */
    uint8_t  interlaced;    /**< Non-zero if interlaced */
    uint32_t pixel_clock;   /**< Pixel clock in Hz */
    uint16_t h_total;       /**< Total horizontal pixels */
    uint16_t v_total;       /**< Total vertical lines */
} video_mode_t;

/*===========================================================================*/
/* Video Interface                                                           */
/*===========================================================================*/

/**
 * @brief Video component configuration
 */
typedef struct {
    void    *framebuffer;   /**< Framebuffer address */
    uint32_t fb_size;       /**< Framebuffer size */
    void    *context;       /**< User context */
} video_config_t;

/**
 * @brief Video component interface (Shifter, VIDEL, etc.)
 */
typedef struct {
    /** Interface version (must be VIDEO_INTERFACE_V1) */
    uint32_t interface_version;
    
    /** Component name (e.g., "Shifter ST", "VIDEL") */
    const char *name;
    
    /*--- Lifecycle ---*/
    
    /** Initialize with configuration */
    int  (*init)(video_config_t *config);
    
    /** Reset to initial state */
    void (*reset)(void);
    
    /** Shutdown and cleanup */
    void (*shutdown)(void);
    
    /*--- Rendering ---*/
    
    /** Render single scanline to buffer (RGB565) */
    void (*render_scanline)(int line, uint16_t *buffer);
    
    /** Render complete frame to framebuffer */
    void (*render_frame)(void *framebuffer);
    
    /*--- Timing ---*/
    
    /** Get current horizontal position (0-based) */
    int  (*get_hpos)(void);
    
    /** Get current vertical position (0-based) */
    int  (*get_vpos)(void);
    
    /** Check if in vertical blank */
    bool (*in_vblank)(void);
    
    /** Check if in horizontal blank */
    bool (*in_hblank)(void);
    
    /** Advance by specified cycles */
    void (*clock)(int cycles);
    
    /*--- Register Access ---*/
    
    /** Read register */
    uint16_t (*read_reg)(uint32_t addr);
    
    /** Write register */
    void     (*write_reg)(uint32_t addr, uint16_t val);
    
    /*--- Mode Info ---*/
    
    /** Get current video mode */
    void (*get_mode)(video_mode_t *mode);
    
    /*--- Bus (for DMA) ---*/
    
    /** Set bus interface */
    void (*set_bus)(bus_interface_t *bus);
    
} video_interface_t;

/*===========================================================================*/
/* Audio Interface                                                           */
/*===========================================================================*/

/**
 * @brief Audio component configuration
 */
typedef struct {
    uint32_t sample_rate;   /**< Output sample rate (e.g., 48000) */
    void    *context;       /**< User context */
} audio_config_t;

/**
 * @brief Audio component interface (YM2149, DMA Sound, DSP)
 */
typedef struct {
    /** Interface version (must be AUDIO_INTERFACE_V1) */
    uint32_t interface_version;
    
    /** Component name (e.g., "YM2149", "DMA Sound") */
    const char *name;
    
    /*--- Lifecycle ---*/
    
    /** Initialize with configuration */
    int  (*init)(audio_config_t *config);
    
    /** Reset to initial state */
    void (*reset)(void);
    
    /** Shutdown and cleanup */
    void (*shutdown)(void);
    
    /*--- Audio Generation ---*/
    
    /** Generate audio samples (stereo interleaved, signed 16-bit) */
    void (*generate)(int16_t *buffer, int samples);
    
    /*--- Timing ---*/
    
    /** Advance by specified cycles */
    void (*clock)(int cycles);
    
    /*--- Register Access ---*/
    
    /** Read register */
    uint8_t (*read_reg)(uint32_t addr);
    
    /** Write register */
    void    (*write_reg)(uint32_t addr, uint8_t val);
    
    /*--- Bus (for DMA audio) ---*/
    
    /** Set bus interface */
    void (*set_bus)(bus_interface_t *bus);
    
} audio_interface_t;

/*===========================================================================*/
/* I/O Interface                                                             */
/*===========================================================================*/

/**
 * @brief I/O component configuration
 */
typedef struct {
    void    *context;       /**< User context */
} io_config_t;

/**
 * @brief I/O component interface (MFP, Blitter, ACIA, etc.)
 */
typedef struct {
    /** Interface version (must be IO_INTERFACE_V1) */
    uint32_t interface_version;
    
    /** Component name (e.g., "MFP 68901", "Blitter") */
    const char *name;
    
    /*--- Lifecycle ---*/
    
    /** Initialize with configuration */
    int  (*init)(io_config_t *config);
    
    /** Reset to initial state */
    void (*reset)(void);
    
    /** Shutdown and cleanup */
    void (*shutdown)(void);
    
    /*--- Register Access ---*/
    
    /** Read byte from register */
    uint8_t  (*read_byte)(uint32_t addr);
    
    /** Read word from register */
    uint16_t (*read_word)(uint32_t addr);
    
    /** Write byte to register */
    void     (*write_byte)(uint32_t addr, uint8_t val);
    
    /** Write word to register */
    void     (*write_word)(uint32_t addr, uint16_t val);
    
    /*--- Timing ---*/
    
    /** Advance by specified cycles */
    void (*clock)(int cycles);
    
    /*--- Interrupts ---*/
    
    /** Check if interrupt pending */
    bool (*irq_pending)(void);
    
    /** Get interrupt vector (for vectored interrupts) */
    uint8_t (*get_vector)(void);
    
    /** Acknowledge interrupt */
    void (*irq_ack)(void);
    
    /*--- Bus (for DMA) ---*/
    
    /** Set bus interface */
    void (*set_bus)(bus_interface_t *bus);
    
    /** Check if holding bus (for blitter) */
    bool (*bus_held)(void);
    
} io_interface_t;

/*===========================================================================*/
/* Unified Monolithic System Interface                                       */
/*===========================================================================*/

/**
 * @brief Interface exposed by a monolithic unified EBIN.
 *
 * A unified EBIN can package CPU, video, audio, and I/O in one artifact.
 * Runtime can request individual sub-interfaces from this contract.
 */
typedef struct {
    /** Interface version (must be SYSTEM_INTERFACE_V1) */
    uint32_t interface_version;

    /** Component name (for example "ST Monolith") */
    const char *name;

    /** Lifecycle */
    int  (*init)(void *config);
    void (*reset)(void);
    void (*shutdown)(void);

    /** Accessors for sub-interfaces */
    cpu_interface_t   *(*get_cpu)(void);
    video_interface_t *(*get_video)(void);
    audio_interface_t *(*get_audio)(int index);
    io_interface_t    *(*get_io)(int index);
} system_interface_t;

/*===========================================================================*/
/* Component Entry Point                                                     */
/*===========================================================================*/

/**
 * @brief Component entry point function type
 * 
 * Each component exports this function at the entry offset.
 * It returns a pointer to the component's interface structure.
 * 
 * @return Pointer to component interface (cpu_interface_t*, etc.)
 */
typedef void* (*component_entry_fn)(void);

#ifdef __cplusplus
}
#endif
