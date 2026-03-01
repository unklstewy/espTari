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
#include <stdio.h>
#include <inttypes.h>
#include "m68000_internal.h"
#include "esptari_memory.h"

/*===========================================================================*/
/* Global State                                                              */
/*===========================================================================*/

m68k_state_t g_cpu;
static int g_level6_vector = -1;

#define MICROTRACE_PC_MIN   0x00FC01B4U
#define MICROTRACE_PC_MAX   0x00FC01D4U
#define MICROTRACE_BUF_SIZE 4096
#define PROBE_FASTFWD_PC_MIN 0x00FC01BCU
#define PROBE_FASTFWD_PC_MAX 0x00FC01CEU
#define PROBE_FASTFWD_A0_MIN 0x00010000U
#define PROBE_FASTFWD_A0_SET 0x00000480U

static char g_microtrace_buf[MICROTRACE_BUF_SIZE];
static int g_microtrace_len = 0;
static uint32_t g_microtrace_seq = 0;
static bool g_microtrace_full = false;
static bool g_probe_fastfwd_done = false;

static void microtrace_reset(void)
{
    g_microtrace_len = 0;
    g_microtrace_seq = 0;
    g_microtrace_full = false;
    g_microtrace_buf[0] = '\0';
}

static void microtrace_append(uint32_t pc_before,
                              uint16_t opcode,
                              uint32_t a0_before,
                              uint32_t a0_after,
                              uint16_t sr_before,
                              uint16_t sr_after,
                              uint32_t d4,
                              uint32_t pc_after)
{
    if (g_microtrace_full) {
        return;
    }

    if (pc_before < MICROTRACE_PC_MIN || pc_before > MICROTRACE_PC_MAX) {
        return;
    }

    int remain = MICROTRACE_BUF_SIZE - g_microtrace_len;
    if (remain <= 96) {
        g_microtrace_full = true;
        return;
    }

    int n = snprintf(&g_microtrace_buf[g_microtrace_len],
                     (size_t)remain,
                     "%03lu pc=%06" PRIX32 " op=%04X a0=%06" PRIX32 "->%06" PRIX32 " sr=%04X->%04X d4=%08" PRIX32 " next=%06" PRIX32 "\n",
                     (unsigned long)g_microtrace_seq,
                     pc_before & 0x00FFFFFFU,
                     (unsigned)opcode,
                     a0_before & 0x00FFFFFFU,
                     a0_after & 0x00FFFFFFU,
                     (unsigned)sr_before,
                     (unsigned)sr_after,
                     d4,
                     pc_after & 0x00FFFFFFU);
    if (n <= 0 || n >= remain) {
        g_microtrace_full = true;
        return;
    }

    g_microtrace_len += n;
    g_microtrace_seq++;
}

int m68k_get_microtrace_text(char *buf, int buf_size)
{
    if (!buf || buf_size <= 0) {
        return g_microtrace_len;
    }

    if (g_microtrace_len <= 0) {
        buf[0] = '\0';
        return 0;
    }

    int copy_len = g_microtrace_len;
    if (copy_len >= (buf_size - 1)) {
        copy_len = buf_size - 1;
    }

    memcpy(buf, g_microtrace_buf, (size_t)copy_len);
    buf[copy_len] = '\0';
    return copy_len;
}

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

static uint64_t bus_error_counter_snapshot(void)
{
    if (!g_cpu.bus.context) {
        return 0;
    }

    const st_memory_t *mem = (const st_memory_t *)g_cpu.bus.context;
    return mem->bus_errors;
}

static void normalize_address_state(void)
{
    g_cpu.pc &= 0x00FFFFFFU;
    g_cpu.usp &= 0x00FFFFFFU;
    g_cpu.ssp &= 0x00FFFFFFU;

    for (int i = 0; i < 8; i++) {
        g_cpu.a[i] &= 0x00FFFFFFU;
    }
}

static void note_bus_error_if_changed(uint64_t before, uint32_t addr)
{
    if (bus_error_counter_snapshot() != before) {
        g_cpu.bus_error_pending = true;
        g_cpu.fault_address = addr;
        g_cpu.fault_write = false;
    }
}

uint8_t m68k_bus_read_byte(uint32_t addr)
{
    uint64_t before = bus_error_counter_snapshot();
    uint8_t value = g_cpu.bus.read_byte(addr);
    note_bus_error_if_changed(before, addr);
    return value;
}

uint16_t m68k_bus_read_word(uint32_t addr)
{
    if (addr & 1U) {
        g_cpu.address_error_pending = true;
        g_cpu.fault_address = addr;
        g_cpu.fault_write = false;
        return 0xFFFF;
    }

    uint64_t before = bus_error_counter_snapshot();
    uint16_t value = g_cpu.bus.read_word(addr);
    note_bus_error_if_changed(before, addr);
    return value;
}

uint32_t m68k_bus_read_long(uint32_t addr)
{
    if (addr & 1U) {
        g_cpu.address_error_pending = true;
        g_cpu.fault_address = addr;
        g_cpu.fault_write = false;
        return 0xFFFFFFFF;
    }

    uint64_t before = bus_error_counter_snapshot();
    uint32_t value = g_cpu.bus.read_long(addr);
    note_bus_error_if_changed(before, addr);
    return value;
}

void m68k_bus_write_byte(uint32_t addr, uint8_t val)
{
    uint64_t before = bus_error_counter_snapshot();
    g_cpu.bus.write_byte(addr, val);
    g_cpu.fault_write = true;
    note_bus_error_if_changed(before, addr);
}

void m68k_bus_write_word(uint32_t addr, uint16_t val)
{
    if (addr & 1U) {
        g_cpu.address_error_pending = true;
        g_cpu.fault_address = addr;
        g_cpu.fault_write = true;
        return;
    }

    uint64_t before = bus_error_counter_snapshot();
    g_cpu.bus.write_word(addr, val);
    g_cpu.fault_write = true;
    note_bus_error_if_changed(before, addr);
}

void m68k_bus_write_long(uint32_t addr, uint32_t val)
{
    if (addr & 1U) {
        g_cpu.address_error_pending = true;
        g_cpu.fault_address = addr;
        g_cpu.fault_write = true;
        return;
    }

    uint64_t before = bus_error_counter_snapshot();
    g_cpu.bus.write_long(addr, val);
    g_cpu.fault_write = true;
    note_bus_error_if_changed(before, addr);
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

    /* Group-0 exceptions (bus/address): push additional frame fields
     * before SR/PC so handlers can inspect fault context. */
    if (vector == VEC_BUS_ERROR || vector == VEC_ADDRESS_ERROR) {
        uint16_t special_status = 0;
        if (!g_cpu.fault_write) {
            special_status |= 0x0010; /* R/W bit: 1 = read, 0 = write */
        }
        if (IS_SUPERVISOR()) {
            special_status |= 0x0004; /* Function code supervisor data */
        }

        push_word(g_cpu.ir);
        push_long(g_cpu.fault_address & 0x00FFFFFFU);
        push_word(special_status);
    }
    
    /* Push PC and SR */
    push_long(g_cpu.pc);
    push_word(old_sr);
    
    /* Read new PC from vector table */
    g_cpu.pc = READ_LONG(vector * 4);
    normalize_address_state();
    
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
        
        /* Process interrupt */
        if (level == 6 && g_level6_vector >= 0) {
            m68k_exception(g_level6_vector & 0xFF);
            g_level6_vector = -1;
        } else {
            m68k_exception(VEC_AUTOVECTOR_1 + level - 1);
        }

        /* Clear pending interrupt; external glue/mfp will reassert
         * on subsequent clocks if the level is still active. */
        g_cpu.pending_irq = 0;
    }
}

void m68k_set_level6_vector(int vector)
{
    g_level6_vector = vector;
}

/*===========================================================================*/
/* Core Execution                                                            */
/*===========================================================================*/

/**
 * @brief Reset the CPU
 */
void m68k_reset(void)
{
    microtrace_reset();
    g_probe_fastfwd_done = false;

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
    g_cpu.bus_error_pending = false;
    g_cpu.address_error_pending = false;
    g_cpu.fault_write = false;
    
    /* Read initial SSP and PC from vector table */
    g_cpu.ssp = READ_LONG(0);
    g_cpu.a[7] = g_cpu.ssp;
    g_cpu.pc = READ_LONG(4);
    normalize_address_state();
    
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

        /* Check for pending bus error */
        if (g_cpu.bus_error_pending) {
            g_cpu.bus_error_pending = false;
            m68k_exception(VEC_BUS_ERROR);
            continue;
        }
        
        /* Check for pending address error */
        if (g_cpu.address_error_pending) {
            g_cpu.address_error_pending = false;
            m68k_exception(VEC_ADDRESS_ERROR);
            continue;
        }
        
        uint32_t pc_before = g_cpu.pc;
        uint32_t a0_before = g_cpu.a[0];
        uint16_t sr_before = g_cpu.sr;

        /* Fetch instruction */
        uint16_t opcode = FETCH_WORD();
        g_cpu.ir = opcode;

        /* Temporary bootstrap accelerator: shorten known RAM probe loop.
         * This is intentionally narrow and one-shot per reset. */
        if (!g_probe_fastfwd_done &&
            pc_before >= PROBE_FASTFWD_PC_MIN && pc_before <= PROBE_FASTFWD_PC_MAX &&
            opcode == 0x48E0U &&
            g_cpu.d[4] == 0x00000400U &&
            g_cpu.a[0] > PROBE_FASTFWD_A0_MIN) {
            g_cpu.a[0] = PROBE_FASTFWD_A0_SET;
            g_probe_fastfwd_done = true;
        }
        
        /* Decode and execute */
        int inst_cycles = m68k_decode_execute(opcode);
        ADD_CYCLES(inst_cycles);

        normalize_address_state();

        microtrace_append(pc_before,
                  opcode,
                  a0_before,
                  g_cpu.a[0],
                  sr_before,
                  g_cpu.sr,
                  g_cpu.d[4],
                  g_cpu.pc);
        
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
void* __attribute__((section(".text.component_entry"))) m68000_entry(void)
{
    return (void*)&s_interface;
}
