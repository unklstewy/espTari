/**
 * @file m68000_internal.h
 * @brief MC68000 CPU Core - Internal Definitions
 * 
 * Internal state, macros, and function declarations for the 68000 core.
 * 
 * SPDX-License-Identifier: MIT
 */

#ifndef M68000_INTERNAL_H
#define M68000_INTERNAL_H

#include <stdint.h>
#include <stdbool.h>

/*===========================================================================*/
/* Status Register Bits                                                      */
/*===========================================================================*/

/* Condition code bits (CCR - low byte of SR) */
#define SR_C    (1 << 0)    /* Carry */
#define SR_V    (1 << 1)    /* Overflow */
#define SR_Z    (1 << 2)    /* Zero */
#define SR_N    (1 << 3)    /* Negative */
#define SR_X    (1 << 4)    /* Extend */

/* System byte (high byte of SR) */
#define SR_IPM  (7 << 8)    /* Interrupt priority mask (bits 8-10) */
#define SR_M    (1 << 12)   /* Master/interrupt state (68020+) */
#define SR_S    (1 << 13)   /* Supervisor state */
#define SR_T0   (1 << 14)   /* Trace bit 0 (68020: T0, 68000: unused) */
#define SR_T1   (1 << 15)   /* Trace bit 1 */

/* 68000 only has T1, called just T */
#define SR_T    SR_T1

/* Interrupt priority mask shift */
#define SR_IPM_SHIFT 8

/*===========================================================================*/
/* Exception Vectors                                                         */
/*===========================================================================*/

#define VEC_RESET_SSP       0   /* Reset: Initial SSP */
#define VEC_RESET_PC        1   /* Reset: Initial PC */
#define VEC_BUS_ERROR       2   /* Bus Error */
#define VEC_ADDRESS_ERROR   3   /* Address Error */
#define VEC_ILLEGAL_INST    4   /* Illegal Instruction */
#define VEC_ZERO_DIVIDE     5   /* Integer Divide by Zero */
#define VEC_CHK             6   /* CHK Instruction */
#define VEC_TRAPV           7   /* TRAPV Instruction */
#define VEC_PRIVILEGE       8   /* Privilege Violation */
#define VEC_TRACE           9   /* Trace */
#define VEC_LINE_A          10  /* Line 1010 Emulator */
#define VEC_LINE_F          11  /* Line 1111 Emulator */
/* 12-14 reserved */
#define VEC_UNINIT_INT      15  /* Uninitialized Interrupt Vector */
/* 16-23 reserved */
#define VEC_SPURIOUS        24  /* Spurious Interrupt */
#define VEC_AUTOVECTOR_1    25  /* Level 1 Autovector */
#define VEC_AUTOVECTOR_2    26  /* Level 2 Autovector */
#define VEC_AUTOVECTOR_3    27  /* Level 3 Autovector */
#define VEC_AUTOVECTOR_4    28  /* Level 4 Autovector */
#define VEC_AUTOVECTOR_5    29  /* Level 5 Autovector */
#define VEC_AUTOVECTOR_6    30  /* Level 6 Autovector */
#define VEC_AUTOVECTOR_7    31  /* Level 7 Autovector (NMI) */
#define VEC_TRAP_BASE       32  /* TRAP #0 - TRAP #15 vectors (32-47) */
/* 48-63 reserved */
#define VEC_USER_BASE       64  /* User vectors (64-255) */

/*===========================================================================*/
/* Addressing Modes                                                          */
/*===========================================================================*/

#define EA_MODE_DREG        0   /* Dn - Data register direct */
#define EA_MODE_AREG        1   /* An - Address register direct */
#define EA_MODE_AREG_IND    2   /* (An) - Address register indirect */
#define EA_MODE_AREG_INC    3   /* (An)+ - Address register indirect with postincrement */
#define EA_MODE_AREG_DEC    4   /* -(An) - Address register indirect with predecrement */
#define EA_MODE_AREG_DISP   5   /* d16(An) - Address register indirect with displacement */
#define EA_MODE_AREG_IDX    6   /* d8(An,Xn) - Address register indirect with index */
#define EA_MODE_OTHER       7   /* Extended addressing modes (use reg field) */

/* Mode 7 extended modes (reg field values) */
#define EA_EXT_ABS_SHORT    0   /* (xxx).W - Absolute short */
#define EA_EXT_ABS_LONG     1   /* (xxx).L - Absolute long */
#define EA_EXT_PC_DISP      2   /* d16(PC) - PC with displacement */
#define EA_EXT_PC_IDX       3   /* d8(PC,Xn) - PC with index */
#define EA_EXT_IMMEDIATE    4   /* #<data> - Immediate */

/*===========================================================================*/
/* Size Encoding                                                             */
/*===========================================================================*/

#define SIZE_BYTE   0
#define SIZE_WORD   1
#define SIZE_LONG   2

/*===========================================================================*/
/* Instruction Timing (68000 base cycles)                                    */
/*===========================================================================*/

/* Effective address calculation times (cycles to add) */
#define EA_TIME_DREG        0
#define EA_TIME_AREG        0
#define EA_TIME_AREG_IND    4
#define EA_TIME_AREG_INC    4
#define EA_TIME_AREG_DEC    6
#define EA_TIME_AREG_DISP   8
#define EA_TIME_AREG_IDX    10
#define EA_TIME_ABS_SHORT   8
#define EA_TIME_ABS_LONG    12
#define EA_TIME_PC_DISP     8
#define EA_TIME_PC_IDX      10
#define EA_TIME_IMMEDIATE   4

/*===========================================================================*/
/* CPU Core State                                                            */
/*===========================================================================*/

/**
 * @brief Internal CPU state
 */
typedef struct {
    /* Main registers */
    uint32_t d[8];          /* Data registers D0-D7 */
    uint32_t a[8];          /* Address registers A0-A7 */
    uint32_t pc;            /* Program counter */
    uint32_t usp;           /* User stack pointer */
    uint32_t ssp;           /* Supervisor stack pointer */
    uint16_t sr;            /* Status register */
    
    /* Execution state */
    uint8_t  stopped;       /* STOP instruction active */
    uint8_t  halted;        /* HALT state */
    int      pending_irq;   /* Pending interrupt level (0-7) */
    
    /* Instruction fetch state */
    uint16_t ir;            /* Instruction register */
    uint32_t prefetch[2];   /* Prefetch queue (2 words) */
    
    /* Cycle counting */
    int      cycles;        /* Cycles in current timeslice */
    int      cycles_left;   /* Cycles remaining in execute() */
    uint64_t total_cycles;  /* Total cycles executed */
    
    /* Configuration */
    uint32_t clock_hz;      /* Clock frequency */
    
    /* Bus interface */
    struct {
        uint8_t  (*read_byte)(uint32_t addr);
        uint16_t (*read_word)(uint32_t addr);
        uint32_t (*read_long)(uint32_t addr);
        void     (*write_byte)(uint32_t addr, uint8_t val);
        void     (*write_word)(uint32_t addr, uint16_t val);
        void     (*write_long)(uint32_t addr, uint32_t val);
        void     (*bus_error)(uint32_t addr, bool write);
        void     (*address_error)(uint32_t addr, bool write);
        void     *context;
    } bus;
    
    /* Exception state */
    uint8_t  exception_pending;
    uint8_t  exception_vector;
    bool     bus_error_pending;
    bool     address_error_pending;
    uint32_t fault_address;
    bool     fault_write;
    
    /* User context */
    void    *context;
} m68k_state_t;

/* Global state (single instance for PIC) */
extern m68k_state_t g_cpu;

/*===========================================================================*/
/* Helper Macros                                                             */
/*===========================================================================*/

/* Register access */
#define REG_D(n)    (g_cpu.d[n])
#define REG_A(n)    (g_cpu.a[n])
#define REG_PC      (g_cpu.pc)
#define REG_SR      (g_cpu.sr)
#define REG_SP      (REG_A(7))

/* Stack pointer based on mode */
#define GET_SP()    ((g_cpu.sr & SR_S) ? g_cpu.ssp : g_cpu.usp)
#define SET_SP(v)   do { \
    if (g_cpu.sr & SR_S) g_cpu.ssp = (v); \
    else g_cpu.usp = (v); \
} while(0)

/* Condition code access */
#define FLAG_C      (g_cpu.sr & SR_C)
#define FLAG_V      (g_cpu.sr & SR_V)
#define FLAG_Z      (g_cpu.sr & SR_Z)
#define FLAG_N      (g_cpu.sr & SR_N)
#define FLAG_X      (g_cpu.sr & SR_X)

/* Set/clear flags */
#define SET_FLAG(f)     (g_cpu.sr |= (f))
#define CLR_FLAG(f)     (g_cpu.sr &= ~(f))
#define SET_FLAG_IF(f, cond) do { \
    if (cond) g_cpu.sr |= (f); else g_cpu.sr &= ~(f); \
} while(0)

/* Supervisor mode */
#define IS_SUPERVISOR() (g_cpu.sr & SR_S)

/* Get interrupt mask */
#define GET_IPM()   ((g_cpu.sr >> SR_IPM_SHIFT) & 7)

/* Sign extension */
#define SEXT8(v)    ((int32_t)(int8_t)(v))
#define SEXT16(v)   ((int32_t)(int16_t)(v))

/* Byte/word/long extraction */
#define BYTE(v)     ((v) & 0xFF)
#define WORD(v)     ((v) & 0xFFFF)
#define LONG(v)     (v)

/* Memory access via bus */
uint8_t  m68k_bus_read_byte(uint32_t addr);
uint16_t m68k_bus_read_word(uint32_t addr);
uint32_t m68k_bus_read_long(uint32_t addr);
void     m68k_bus_write_byte(uint32_t addr, uint8_t val);
void     m68k_bus_write_word(uint32_t addr, uint16_t val);
void     m68k_bus_write_long(uint32_t addr, uint32_t val);

#define READ_BYTE(a)    (m68k_bus_read_byte(a))
#define READ_WORD(a)    (m68k_bus_read_word(a))
#define READ_LONG(a)    (m68k_bus_read_long(a))
#define WRITE_BYTE(a,v) (m68k_bus_write_byte(a, v))
#define WRITE_WORD(a,v) (m68k_bus_write_word(a, v))
#define WRITE_LONG(a,v) (m68k_bus_write_long(a, v))

/* Fetch instruction word and advance PC */
#define FETCH_WORD()    (g_cpu.pc += 2, READ_WORD(g_cpu.pc - 2))

/* Add cycles */
#define ADD_CYCLES(n)   (g_cpu.cycles += (n))

/*===========================================================================*/
/* Function Declarations                                                     */
/*===========================================================================*/

/* Core functions (m68000.c) */
void m68k_reset(void);
int  m68k_execute(int cycles);
void m68k_exception(int vector);
void m68k_set_irq(int level);
void m68k_set_level6_vector(int vector);

/* Instruction decoder/executor (m68000_ops.c) */
int m68k_decode_execute(uint16_t opcode);

/* Effective address handling (m68000_ea.c) */
uint32_t m68k_get_ea(int mode, int reg, int size);
uint32_t m68k_read_ea(int mode, int reg, int size);
void     m68k_write_ea(int mode, int reg, int size, uint32_t value);

/* Flag calculation */
void m68k_set_flags_add(int size, uint32_t src, uint32_t dst, uint32_t res);
void m68k_set_flags_sub(int size, uint32_t src, uint32_t dst, uint32_t res);
void m68k_set_flags_logic(int size, uint32_t res);
void m68k_set_flags_move(int size, uint32_t res);

#endif /* M68000_INTERNAL_H */
