/**
 * @file m68000.c
 * @brief MC68000 CPU Core - Main Implementation
 * 
 * Motorola 68000 CPU emulation for espTari. This is a loadable
 * EBIN component implementing the cpu_interface_t.
 * 
 * Features:
 * - Full 68000 instruction set
 * - Cycle-accurate timing
 * - All addressing modes
 * - Exception handling
 * - Trace mode
 * 
 * SPDX-License-Identifier: MIT
 */

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "m68000_internal.h"

/*===========================================================================*/
/* Global State                                                              */
/*===========================================================================*/

m68k_state_t g_cpu;

/*===========================================================================*/
/* Bus Interface (set by system)                                             */
/*===========================================================================*/

static uint8_t null_read_byte(uint32_t addr) 
{ 
    (void)addr; 
    return 0xFF; 
}

static uint16_t null_read_word(uint32_t addr) 
{ 
    (void)addr; 
    return 0xFFFF; 
}

static uint32_t null_read_long(uint32_t addr) 
{ 
    (void)addr; 
    return 0xFFFFFFFF; 
}

static void null_write_byte(uint32_t addr, uint8_t val) 
{ 
    (void)addr; 
    (void)val; 
}

static void null_write_word(uint32_t addr, uint16_t val) 
{ 
    (void)addr; 
    (void)val; 
}

static void null_write_long(uint32_t addr, uint32_t val) 
{ 
    (void)addr; 
    (void)val; 
}

static void null_bus_error(uint32_t addr, bool write)
{
    (void)addr;
    (void)write;
}

static void null_address_error(uint32_t addr, bool write)
{
    (void)addr;
    (void)write;
}

/*===========================================================================*/
/* Internal Functions                                                        */
/*===========================================================================*/

/**
 * @brief Set CPU to supervisor mode and swap stack pointers
 */
static void enter_supervisor(void)
{
    if (!(g_cpu.sr & SR_S)) {
        /* Switching from user to supervisor */
        g_cpu.usp = g_cpu.a[7];
        g_cpu.a[7] = g_cpu.ssp;
        g_cpu.sr |= SR_S;
    }
}

/**
 * @brief Exit supervisor mode
 */
static void exit_supervisor(void)
{
    if (g_cpu.sr & SR_S) {
        /* Switching from supervisor to user */
        g_cpu.ssp = g_cpu.a[7];
        g_cpu.a[7] = g_cpu.usp;
        g_cpu.sr &= ~SR_S;
    }
}

/**
 * @brief Push value onto stack
 */
static void push_long(uint32_t value)
{
    g_cpu.a[7] -= 4;
    WRITE_LONG(g_cpu.a[7], value);
}

static void push_word(uint16_t value)
{
    g_cpu.a[7] -= 2;
    WRITE_WORD(g_cpu.a[7], value);
}

/**
 * @brief Pop value from stack
 */
static uint32_t pop_long(void)
{
    uint32_t val = READ_LONG(g_cpu.a[7]);
    g_cpu.a[7] += 4;
    return val;
}

static uint16_t pop_word(void)
{
    uint16_t val = READ_WORD(g_cpu.a[7]);
    g_cpu.a[7] += 2;
    return val;
}

/*===========================================================================*/
/* Exception Handling                                                        */
/*===========================================================================*/

/**
 * @brief Process an exception
 * 
 * General exception processing:
 * 1. Copy SR to temp
 * 2. Set supervisor mode
 * 3. Clear trace bits
 * 4. Push PC (long)
 * 5. Push SR (word)
 * 6. Read new PC from vector table
 */
void m68k_exception(int vector)
{
    uint16_t old_sr = g_cpu.sr;
    
    /* Enter supervisor mode */
    enter_supervisor();
    
    /* Clear trace bits */
    g_cpu.sr &= ~(SR_T1 | SR_T0);
    
    /* For group 0 exceptions (reset, bus error, address error),
     * additional information is pushed */
    if (vector == VEC_ADDRESS_ERROR || vector == VEC_BUS_ERROR) {
        /* Push additional info (simplified - full format is more complex) */
        push_word(g_cpu.ir);                    /* Instruction register */
        push_long(g_cpu.fault_address);         /* Access address */
        push_word(0);                           /* Function code + R/W */
    }
    
    /* Push PC and SR */
    push_long(g_cpu.pc);
    push_word(old_sr);
    
    /* Read new PC from vector table */
    g_cpu.pc = READ_LONG(vector * 4);
    
    /* Add cycles for exception processing */
    /* Group 0: 50 cycles, Group 1/2: 34 cycles */
    if (vector <= VEC_ADDRESS_ERROR) {
        ADD_CYCLES(50);
    } else {
        ADD_CYCLES(34);
    }
    
    /* Clear stopped state */
    g_cpu.stopped = 0;
}

/**
 * @brief Check and process pending interrupts
 */
static void check_interrupts(void)
{
    if (g_cpu.pending_irq == 0) return;
    
    /* Get interrupt mask from SR */
    int mask = GET_IPM();
    
    /* Level 7 is NMI (non-maskable), always processed */
    /* Other levels must be higher than current mask */
    if (g_cpu.pending_irq == 7 || g_cpu.pending_irq > mask) {
        /* Accept the interrupt */
        int level = g_cpu.pending_irq;
        
        /* Update interrupt mask */
        g_cpu.sr = (g_cpu.sr & ~SR_IPM) | (level << SR_IPM_SHIFT);
        
        /* Process autovector interrupt */
        m68k_exception(VEC_AUTOVECTOR_1 + level - 1);
        
        /* Clear pending interrupt (edge-triggered for level 7) */
        if (level == 7) {
            g_cpu.pending_irq = 0;
        }
    }
}

/*===========================================================================*/
/* Core Execution                                                            */
/*===========================================================================*/

/**
 * @brief Reset the CPU
 */
void m68k_reset(void)
{
    /* Clear all registers */
    for (int i = 0; i < 8; i++) {
        g_cpu.d[i] = 0;
        g_cpu.a[i] = 0;
    }
    
    /* Set supervisor mode with interrupts masked */
    g_cpu.sr = SR_S | SR_IPM;
    
    /* Clear execution state */
    g_cpu.stopped = 0;
    g_cpu.halted = 0;
    g_cpu.pending_irq = 0;
    g_cpu.exception_pending = 0;
    g_cpu.address_error_pending = false;
    
    /* Read initial SSP and PC from vector table */
    g_cpu.ssp = READ_LONG(0);
    g_cpu.a[7] = g_cpu.ssp;
    g_cpu.pc = READ_LONG(4);
    
    /* Reset cycle count */
    g_cpu.cycles = 0;
    g_cpu.total_cycles = 0;
}

/**
 * @brief Execute instructions for given number of cycles
 * @param cycles Maximum cycles to execute
 * @return Actual cycles executed (may be slightly more)
 */
int m68k_execute(int cycles)
{
    g_cpu.cycles = 0;
    g_cpu.cycles_left = cycles;
    
    while (g_cpu.cycles < cycles) {
        /* Check for halt state */
        if (g_cpu.halted) {
            g_cpu.cycles = cycles;
            break;
        }
        
        /* Check for stop state */
        if (g_cpu.stopped) {
            /* Still check interrupts in stopped state */
            check_interrupts();
            if (g_cpu.stopped) {
                ADD_CYCLES(4);  /* STOP adds 4 cycles per check */
                continue;
            }
        }
        
        /* Check pending interrupts */
        check_interrupts();
        
        /* Check for pending address error */
        if (g_cpu.address_error_pending) {
            g_cpu.address_error_pending = false;
            m68k_exception(VEC_ADDRESS_ERROR);
            continue;
        }
        
        /* Fetch instruction */
        uint16_t opcode = FETCH_WORD();
        g_cpu.ir = opcode;
        
        /* Decode and execute */
        int inst_cycles = m68k_decode_execute(opcode);
        ADD_CYCLES(inst_cycles);
        
        /* Check for trace mode */
        if (g_cpu.sr & SR_T) {
            m68k_exception(VEC_TRACE);
        }
    }
    
    g_cpu.total_cycles += g_cpu.cycles;
    return g_cpu.cycles;
}

/**
 * @brief Set interrupt request level
 */
void m68k_set_irq(int level)
{
    if (level < 0) level = 0;
    if (level > 7) level = 7;
    g_cpu.pending_irq = level;
}

/*===========================================================================*/
/* Flag Calculation Helpers                                                  */
/*===========================================================================*/

/**
 * @brief Set flags for ADD operation
 */
void m68k_set_flags_add(int size, uint32_t src, uint32_t dst, uint32_t res)
{
    uint32_t mask, msb;
    
    switch (size) {
        case SIZE_BYTE: mask = 0xFF; msb = 0x80; break;
        case SIZE_WORD: mask = 0xFFFF; msb = 0x8000; break;
        default:        mask = 0xFFFFFFFF; msb = 0x80000000; break;
    }
    
    src &= mask;
    dst &= mask;
    res &= mask;
    
    /* N - Set if result is negative */
    SET_FLAG_IF(SR_N, res & msb);
    
    /* Z - Set if result is zero */
    SET_FLAG_IF(SR_Z, res == 0);
    
    /* V - Set if overflow occurred */
    /* Overflow: both operands same sign, result different sign */
    uint32_t overflow = ((src ^ res) & (dst ^ res)) & msb;
    SET_FLAG_IF(SR_V, overflow);
    
    /* C - Set if carry occurred */
    /* For add: carry if result < either operand */
    SET_FLAG_IF(SR_C, res < src);
    
    /* X - Set the same as C */
    SET_FLAG_IF(SR_X, res < src);
}

/**
 * @brief Set flags for SUB operation
 */
void m68k_set_flags_sub(int size, uint32_t src, uint32_t dst, uint32_t res)
{
    uint32_t mask, msb;
    
    switch (size) {
        case SIZE_BYTE: mask = 0xFF; msb = 0x80; break;
        case SIZE_WORD: mask = 0xFFFF; msb = 0x8000; break;
        default:        mask = 0xFFFFFFFF; msb = 0x80000000; break;
    }
    
    src &= mask;
    dst &= mask;
    res &= mask;
    
    /* N - Set if result is negative */
    SET_FLAG_IF(SR_N, res & msb);
    
    /* Z - Set if result is zero */
    SET_FLAG_IF(SR_Z, res == 0);
    
    /* V - Set if overflow occurred */
    /* For sub: overflow if operands different sign and result sign differs from dst */
    uint32_t overflow = ((src ^ dst) & (dst ^ res)) & msb;
    SET_FLAG_IF(SR_V, overflow);
    
    /* C - Set if borrow occurred */
    SET_FLAG_IF(SR_C, src > dst);
    
    /* X - Set the same as C */
    SET_FLAG_IF(SR_X, src > dst);
}

/**
 * @brief Set flags for logical operations (AND, OR, EOR)
 */
void m68k_set_flags_logic(int size, uint32_t res)
{
    uint32_t mask, msb;
    
    switch (size) {
        case SIZE_BYTE: mask = 0xFF; msb = 0x80; break;
        case SIZE_WORD: mask = 0xFFFF; msb = 0x8000; break;
        default:        mask = 0xFFFFFFFF; msb = 0x80000000; break;
    }
    
    res &= mask;
    
    SET_FLAG_IF(SR_N, res & msb);
    SET_FLAG_IF(SR_Z, res == 0);
    CLR_FLAG(SR_V);
    CLR_FLAG(SR_C);
}

/**
 * @brief Set flags for MOVE operation
 */
void m68k_set_flags_move(int size, uint32_t res)
{
    m68k_set_flags_logic(size, res);
}

/*===========================================================================*/
/* Component Interface Implementation                                        */
/*===========================================================================*/

/* Forward declare interface definition structure */
typedef struct {
    void *context;
} simple_cpu_config_t;

typedef struct {
    uint32_t d[8];
    uint32_t a[8];
    uint32_t pc;
    uint32_t usp;
    uint32_t ssp;
    uint32_t msp;
    uint32_t isp;
    uint16_t sr;
    uint32_t vbr;
    uint32_t cacr;
    uint32_t caar;
    uint32_t sfc;
    uint32_t dfc;
    uint8_t  stopped;
    uint8_t  halted;
    int      pending_irq;
    uint64_t cycles;
} external_cpu_state_t;

/** Bus interface pointer set by system */
static struct {
    uint8_t  (*read_byte)(uint32_t addr);
    uint16_t (*read_word)(uint32_t addr);
    uint32_t (*read_long)(uint32_t addr);
    void     (*write_byte)(uint32_t addr, uint8_t val);
    void     (*write_word)(uint32_t addr, uint16_t val);
    void     (*write_long)(uint32_t addr, uint32_t val);
    void     (*bus_error)(uint32_t addr, bool write);
    void     (*address_error)(uint32_t addr, bool write);
    void     *context;
} *s_bus_interface = (void*)0;

/**
 * @brief Initialize CPU
 */
static int cpu_init(simple_cpu_config_t *config)
{
    /* Initialize default bus handlers */
    g_cpu.bus.read_byte = null_read_byte;
    g_cpu.bus.read_word = null_read_word;
    g_cpu.bus.read_long = null_read_long;
    g_cpu.bus.write_byte = null_write_byte;
    g_cpu.bus.write_word = null_write_word;
    g_cpu.bus.write_long = null_write_long;
    g_cpu.bus.bus_error = null_bus_error;
    g_cpu.bus.address_error = null_address_error;
    
    if (config) {
        g_cpu.context = config->context;
    }
    
    return 0;
}

/**
 * @brief Reset CPU
 */
static void cpu_reset(void)
{
    m68k_reset();
}

/**
 * @brief Shutdown CPU
 */
static void cpu_shutdown(void)
{
    /* Nothing to clean up */
}

/**
 * @brief Execute cycles
 */
static int cpu_execute(int cycles)
{
    return m68k_execute(cycles);
}

/**
 * @brief Stop execution
 */
static void cpu_stop(void)
{
    g_cpu.halted = 1;
}

/**
 * @brief Get CPU state
 */
static void cpu_get_state(external_cpu_state_t *state)
{
    for (int i = 0; i < 8; i++) {
        state->d[i] = g_cpu.d[i];
        state->a[i] = g_cpu.a[i];
    }
    state->pc = g_cpu.pc;
    state->usp = g_cpu.usp;
    state->ssp = g_cpu.ssp;
    state->msp = 0;  /* Not used on 68000 */
    state->isp = 0;  /* Not used on 68000 */
    state->sr = g_cpu.sr;
    state->vbr = 0;  /* Not present on 68000 */
    state->cacr = 0;
    state->caar = 0;
    state->sfc = 0;
    state->dfc = 0;
    state->stopped = g_cpu.stopped;
    state->halted = g_cpu.halted;
    state->pending_irq = g_cpu.pending_irq;
    state->cycles = g_cpu.total_cycles;
}

/**
 * @brief Set CPU state
 */
static void cpu_set_state(const external_cpu_state_t *state)
{
    for (int i = 0; i < 8; i++) {
        g_cpu.d[i] = state->d[i];
        g_cpu.a[i] = state->a[i];
    }
    g_cpu.pc = state->pc;
    g_cpu.usp = state->usp;
    g_cpu.ssp = state->ssp;
    g_cpu.sr = state->sr;
    g_cpu.stopped = state->stopped;
    g_cpu.halted = state->halted;
    g_cpu.pending_irq = state->pending_irq;
    g_cpu.total_cycles = state->cycles;
}

/**
 * @brief Set IRQ level
 */
static void cpu_set_irq(int level)
{
    m68k_set_irq(level);
}

/**
 * @brief Trigger NMI
 */
static void cpu_set_nmi(void)
{
    m68k_set_irq(7);
}

/**
 * @brief Set bus interface
 */
static void cpu_set_bus(void *bus)
{
    if (bus) {
        /* Copy function pointers from bus interface */
        uint8_t  (**rb)(uint32_t) = bus;
        g_cpu.bus.read_byte = rb[0];
        g_cpu.bus.read_word = (uint16_t (*)(uint32_t))((void**)bus)[1];
        g_cpu.bus.read_long = (uint32_t (*)(uint32_t))((void**)bus)[2];
        g_cpu.bus.write_byte = (void (*)(uint32_t, uint8_t))((void**)bus)[3];
        g_cpu.bus.write_word = (void (*)(uint32_t, uint16_t))((void**)bus)[4];
        g_cpu.bus.write_long = (void (*)(uint32_t, uint32_t))((void**)bus)[5];
        g_cpu.bus.bus_error = (void (*)(uint32_t, bool))((void**)bus)[6];
        g_cpu.bus.address_error = (void (*)(uint32_t, bool))((void**)bus)[7];
        g_cpu.bus.context = ((void**)bus)[8];
    }
}

/*===========================================================================*/
/* Interface Structure                                                       */
/*===========================================================================*/

static const struct {
    uint32_t    interface_version;
    const char *name;
    uint32_t    features;
    
    int  (*init)(simple_cpu_config_t *config);
    void (*reset)(void);
    void (*shutdown)(void);
    
    int  (*execute)(int cycles);
    void (*stop)(void);
    
    void (*get_state)(external_cpu_state_t *state);
    void (*set_state)(const external_cpu_state_t *state);
    
    void (*set_irq)(int level);
    void (*set_nmi)(void);
    
    void (*set_bus)(void *bus);
    
    /* Debug functions - NULL for now */
    void *disassemble;
    void *set_breakpoint;
    void *clear_breakpoint;
    void *step;
} s_interface = {
    .interface_version = 0x00010000,  /* CPU_INTERFACE_V1 */
    .name              = "MC68000",
    .features          = 0,           /* No FPU, MMU, etc. */
    
    .init              = cpu_init,
    .reset             = cpu_reset,
    .shutdown          = cpu_shutdown,
    
    .execute           = cpu_execute,
    .stop              = cpu_stop,
    
    .get_state         = cpu_get_state,
    .set_state         = cpu_set_state,
    
    .set_irq           = cpu_set_irq,
    .set_nmi           = cpu_set_nmi,
    
    .set_bus           = cpu_set_bus,
    
    .disassemble       = (void*)0,
    .set_breakpoint    = (void*)0,
    .clear_breakpoint  = (void*)0,
    .step              = (void*)0,
};

/*===========================================================================*/
/* Entry Point                                                               */
/*===========================================================================*/

/**
 * @brief Component entry point
 * 
 * Called by loader after relocations are applied.
 * Returns pointer to the interface structure.
 */
void* __attribute__((section(".text.component_entry"))) component_entry(void)
{
    return (void*)&s_interface;
}
