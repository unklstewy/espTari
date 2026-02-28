/**
 * @file m68000_ops.c
 * @brief MC68000 CPU Core - Instruction Decoder and Executor
 * 
 * Decodes and executes MC68000 instructions. The 68000 uses a
 * 16-bit opcode where the top 4 bits determine the instruction group.
 * 
 * Instruction Groups (bits 15-12):
 *   0000 - Bit manipulation/MOVEP/Immediate
 *   0001 - Move byte
 *   0010 - Move long
 *   0011 - Move word
 *   0100 - Miscellaneous
 *   0101 - ADDQ/SUBQ/Scc/DBcc
 *   0110 - Bcc/BSR
 *   0111 - MOVEQ
 *   1000 - OR/DIV/SBCD
 *   1001 - SUB/SUBX
 *   1010 - (Line A - unassigned)
 *   1011 - CMP/EOR
 *   1100 - AND/MUL/ABCD/EXG
 *   1101 - ADD/ADDX
 *   1110 - Shift/Rotate
 *   1111 - (Line F - coprocessor)
 * 
 * SPDX-License-Identifier: MIT
 */

#include "m68000_internal.h"

/*===========================================================================*/
/* Opcode Field Extraction                                                   */
/*===========================================================================*/

/* Extract fields from opcode */
#define OP_GROUP(op)    (((op) >> 12) & 0xF)
#define OP_REG_DST(op)  (((op) >> 9) & 0x7)
#define OP_MODE_DST(op) (((op) >> 6) & 0x7)
#define OP_MODE_SRC(op) (((op) >> 3) & 0x7)
#define OP_REG_SRC(op)  ((op) & 0x7)
#define OP_SIZE(op)     (((op) >> 6) & 0x3)
#define OP_COND(op)     (((op) >> 8) & 0xF)

/*===========================================================================*/
/* Condition Code Evaluation                                                 */
/*===========================================================================*/

/**
 * @brief Evaluate condition code
 * @param cc Condition code (0-15)
 * @return true if condition is true
 */
static bool eval_condition(int cc)
{
    bool n = (g_cpu.sr & SR_N) != 0;
    bool z = (g_cpu.sr & SR_Z) != 0;
    bool v = (g_cpu.sr & SR_V) != 0;
    bool c = (g_cpu.sr & SR_C) != 0;
    
    switch (cc) {
        case 0:  return true;           /* T  - True */
        case 1:  return false;          /* F  - False */
        case 2:  return !c && !z;       /* HI - Higher */
        case 3:  return c || z;         /* LS - Lower or Same */
        case 4:  return !c;             /* CC - Carry Clear */
        case 5:  return c;              /* CS - Carry Set */
        case 6:  return !z;             /* NE - Not Equal */
        case 7:  return z;              /* EQ - Equal */
        case 8:  return !v;             /* VC - Overflow Clear */
        case 9:  return v;              /* VS - Overflow Set */
        case 10: return !n;             /* PL - Plus */
        case 11: return n;              /* MI - Minus */
        case 12: return n == v;         /* GE - Greater or Equal */
        case 13: return n != v;         /* LT - Less Than */
        case 14: return !z && (n == v); /* GT - Greater Than */
        case 15: return z || (n != v);  /* LE - Less or Equal */
        default: return false;
    }
}

/*===========================================================================*/
/* Group 0: Bit operations / Immediate                                       */
/*===========================================================================*/

static int op_group_0(uint16_t opcode)
{
    int cycles = 4;  /* Base cycles */
    
    /* Check for immediate operations */
    if ((opcode & 0x0100) == 0) {
        /* ORI, ANDI, SUBI, ADDI, EORI, CMPI */
        int op = (opcode >> 9) & 0x7;
        int size_bits = OP_SIZE(opcode);
        int size;
        
        switch (size_bits) {
            case 0: size = SIZE_BYTE; break;
            case 1: size = SIZE_WORD; break;
            case 2: size = SIZE_LONG; break;
            default: goto illegal;
        }
        
        int mode = OP_MODE_SRC(opcode);
        int reg = OP_REG_SRC(opcode);
        
        /* Fetch immediate value */
        uint32_t imm;
        if (size == SIZE_LONG) {
            uint32_t hi = FETCH_WORD();
            uint32_t lo = FETCH_WORD();
            imm = (hi << 16) | lo;
            cycles += 8;
        } else {
            imm = FETCH_WORD();
            if (size == SIZE_BYTE) imm &= 0xFF;
            cycles += 4;
        }
        
        /* Special case: immediate to SR/CCR */
        if (mode == 7 && reg == 4) {
            if (op == 0) {
                /* ORI to CCR/SR */
                if (size == SIZE_BYTE) {
                    g_cpu.sr |= (imm & 0xFF);
                } else {
                    if (!IS_SUPERVISOR()) {
                        m68k_exception(VEC_PRIVILEGE);
                        return 34;
                    }
                    g_cpu.sr |= imm;
                }
                return cycles + 8;
            } else if (op == 1) {
                /* ANDI to CCR/SR */
                if (size == SIZE_BYTE) {
                    g_cpu.sr &= (imm | 0xFF00);
                } else {
                    if (!IS_SUPERVISOR()) {
                        m68k_exception(VEC_PRIVILEGE);
                        return 34;
                    }
                    g_cpu.sr &= imm;
                }
                return cycles + 8;
            } else if (op == 5) {
                /* EORI to CCR/SR */
                if (size == SIZE_BYTE) {
                    g_cpu.sr ^= (imm & 0xFF);
                } else {
                    if (!IS_SUPERVISOR()) {
                        m68k_exception(VEC_PRIVILEGE);
                        return 34;
                    }
                    g_cpu.sr ^= imm;
                }
                return cycles + 8;
            }
        }
        
        /* Read destination */
        uint32_t dst = m68k_read_ea(mode, reg, size);
        uint32_t res;
        
        switch (op) {
            case 0: /* ORI */
                res = dst | imm;
                m68k_set_flags_logic(size, res);
                m68k_write_ea(mode, reg, size, res);
                break;
                
            case 1: /* ANDI */
                res = dst & imm;
                m68k_set_flags_logic(size, res);
                m68k_write_ea(mode, reg, size, res);
                break;
                
            case 2: /* SUBI */
                res = dst - imm;
                m68k_set_flags_sub(size, imm, dst, res);
                m68k_write_ea(mode, reg, size, res);
                break;
                
            case 3: /* ADDI */
                res = dst + imm;
                m68k_set_flags_add(size, imm, dst, res);
                m68k_write_ea(mode, reg, size, res);
                break;
                
            case 5: /* EORI */
                res = dst ^ imm;
                m68k_set_flags_logic(size, res);
                m68k_write_ea(mode, reg, size, res);
                break;
                
            case 6: /* CMPI */
                res = dst - imm;
                m68k_set_flags_sub(size, imm, dst, res);
                /* Don't write result */
                break;
                
            default:
                goto illegal;
        }
        
        cycles += (size == SIZE_LONG) ? 8 : 4;
        if (mode != EA_MODE_DREG) cycles += 4;
        
        return cycles;
    }
    
    /* BTST, BCHG, BCLR, BSET - register */
    if ((opcode & 0x01C0) == 0x0100 ||
        (opcode & 0x01C0) == 0x0140 ||
        (opcode & 0x01C0) == 0x0180 ||
        (opcode & 0x01C0) == 0x01C0) {
        
        int bit_op = (opcode >> 6) & 0x3;
        int mode = OP_MODE_SRC(opcode);
        int reg = OP_REG_SRC(opcode);
        int dreg = OP_REG_DST(opcode);
        
        uint32_t bit_num = REG_D(dreg);
        
        /* Determine size: byte for memory, long for register */
        int size;
        if (mode == EA_MODE_DREG) {
            size = SIZE_LONG;
            bit_num &= 31;
        } else {
            size = SIZE_BYTE;
            bit_num &= 7;
        }
        
        uint32_t data = m68k_read_ea(mode, reg, size);
        uint32_t mask = 1 << bit_num;
        
        /* Z flag is set to complement of tested bit */
        SET_FLAG_IF(SR_Z, !(data & mask));
        
        switch (bit_op) {
            case 0: /* BTST - just test */
                break;
            case 1: /* BCHG - toggle */
                data ^= mask;
                m68k_write_ea(mode, reg, size, data);
                break;
            case 2: /* BCLR - clear */
                data &= ~mask;
                m68k_write_ea(mode, reg, size, data);
                break;
            case 3: /* BSET - set */
                data |= mask;
                m68k_write_ea(mode, reg, size, data);
                break;
        }
        
        return (size == SIZE_LONG) ? 8 : 6;
    }
    
    /* BTST, BCHG, BCLR, BSET - immediate bit number */
    if ((opcode & 0x0F00) == 0x0800) {
        int bit_op = (opcode >> 6) & 0x3;
        int mode = OP_MODE_SRC(opcode);
        int reg = OP_REG_SRC(opcode);
        
        uint32_t bit_num = FETCH_WORD() & 0xFF;
        
        int size;
        if (mode == EA_MODE_DREG) {
            size = SIZE_LONG;
            bit_num &= 31;
        } else {
            size = SIZE_BYTE;
            bit_num &= 7;
        }
        
        uint32_t data = m68k_read_ea(mode, reg, size);
        uint32_t mask = 1 << bit_num;
        
        SET_FLAG_IF(SR_Z, !(data & mask));
        
        switch (bit_op) {
            case 0: /* BTST */
                break;
            case 1: /* BCHG */
                data ^= mask;
                m68k_write_ea(mode, reg, size, data);
                break;
            case 2: /* BCLR */
                data &= ~mask;
                m68k_write_ea(mode, reg, size, data);
                break;
            case 3: /* BSET */
                data |= mask;
                m68k_write_ea(mode, reg, size, data);
                break;
        }
        
        return (size == SIZE_LONG) ? 12 : 10;
    }
    
illegal:
    m68k_exception(VEC_ILLEGAL_INST);
    return 34;
}

/*===========================================================================*/
/* Groups 1-3: MOVE                                                          */
/*===========================================================================*/

static int op_move(uint16_t opcode)
{
    int size_code = OP_GROUP(opcode);
    int size;
    int cycles = 4;
    
    switch (size_code) {
        case 1: size = SIZE_BYTE; break;
        case 2: size = SIZE_LONG; cycles = 4; break;
        case 3: size = SIZE_WORD; break;
        default: goto illegal;
    }
    
    int src_mode = OP_MODE_SRC(opcode);
    int src_reg = OP_REG_SRC(opcode);
    int dst_mode = OP_MODE_DST(opcode);
    int dst_reg = OP_REG_DST(opcode);
    
    /* Read source */
    uint32_t value = m68k_read_ea(src_mode, src_reg, size);
    
    /* Write destination */
    /* Note: for MOVE, destination mode/reg are swapped in encoding */
    m68k_write_ea(dst_mode, dst_reg, size, value);
    
    /* Set flags (N, Z cleared; V, C cleared) */
    m68k_set_flags_move(size, value);
    
    /* Add timing for long word operations */
    if (size == SIZE_LONG) {
        cycles += 4;
    }
    
    return cycles;
    
illegal:
    m68k_exception(VEC_ILLEGAL_INST);
    return 34;
}

/*===========================================================================*/
/* Group 4: Miscellaneous                                                    */
/*===========================================================================*/

static int op_group_4(uint16_t opcode)
{
    int cycles = 4;
    
    /* NEGX, CLR, NEG, NOT, EXT, NBCD, SWAP, PEA, ILLEGAL, TAS,
     * TST, MOVEM, LEA, CHK, JMP, JSR, LINK, UNLK, MOVE USP, etc. */
    
    int subop = (opcode >> 8) & 0xF;
    int mode = OP_MODE_SRC(opcode);
    int reg = OP_REG_SRC(opcode);
    
    switch (subop) {
        case 0x0: /* NEGX */
        case 0x2: /* CLR */
        case 0x4: /* NEG */
        case 0x6: /* NOT */
        {
            int size;
            switch ((opcode >> 6) & 0x3) {
                case 0: size = SIZE_BYTE; break;
                case 1: size = SIZE_WORD; break;
                case 2: size = SIZE_LONG; break;
                default: goto illegal;
            }
            
            if (subop == 0x2) {
                /* CLR */
                m68k_write_ea(mode, reg, size, 0);
                g_cpu.sr &= ~(SR_N | SR_V | SR_C);
                g_cpu.sr |= SR_Z;
                cycles = (size == SIZE_LONG) ? 6 : 4;
                if (mode != EA_MODE_DREG) cycles += 4;
            } else if (subop == 0x4) {
                /* NEG */
                uint32_t val = m68k_read_ea(mode, reg, size);
                uint32_t res = 0 - val;
                m68k_set_flags_sub(size, val, 0, res);
                m68k_write_ea(mode, reg, size, res);
                cycles = (size == SIZE_LONG) ? 6 : 4;
            } else if (subop == 0x6) {
                /* NOT */
                uint32_t val = m68k_read_ea(mode, reg, size);
                uint32_t res = ~val;
                m68k_set_flags_logic(size, res);
                m68k_write_ea(mode, reg, size, res);
                cycles = (size == SIZE_LONG) ? 6 : 4;
            } else {
                /* NEGX */
                uint32_t val = m68k_read_ea(mode, reg, size);
                uint32_t x = (g_cpu.sr & SR_X) ? 1 : 0;
                uint32_t res = 0 - val - x;
                
                /* NEGX: Z only cleared if result non-zero */
                uint32_t mask, msb;
                switch (size) {
                    case SIZE_BYTE: mask = 0xFF; msb = 0x80; break;
                    case SIZE_WORD: mask = 0xFFFF; msb = 0x8000; break;
                    default: mask = 0xFFFFFFFF; msb = 0x80000000; break;
                }
                res &= mask;
                
                SET_FLAG_IF(SR_N, res & msb);
                if (res != 0) CLR_FLAG(SR_Z);  /* Z cleared only if non-zero */
                SET_FLAG_IF(SR_V, (val & res) & msb);
                SET_FLAG_IF(SR_C, val != 0 || x != 0);
                SET_FLAG_IF(SR_X, val != 0 || x != 0);
                
                m68k_write_ea(mode, reg, size, res);
                cycles = (size == SIZE_LONG) ? 6 : 4;
            }
            break;
        }
        
        case 0x8:
            if ((opcode & 0x00C0) == 0x0040) {
                /* SWAP */
                if (mode == 0) {
                    uint32_t val = REG_D(reg);
                    REG_D(reg) = (val >> 16) | (val << 16);
                    m68k_set_flags_logic(SIZE_LONG, REG_D(reg));
                    cycles = 4;
                } else {
                    /* EXT */
                    int sign_size = ((opcode >> 6) & 1) ? SIZE_WORD : SIZE_BYTE;
                    if (sign_size == SIZE_BYTE) {
                        /* EXT.W - extend byte to word */
                        REG_D(reg) = (REG_D(reg) & 0xFFFF0000) | 
                                     (uint16_t)SEXT8(REG_D(reg));
                        m68k_set_flags_logic(SIZE_WORD, REG_D(reg) & 0xFFFF);
                    } else {
                        /* EXT.L - extend word to long */
                        REG_D(reg) = SEXT16(REG_D(reg));
                        m68k_set_flags_logic(SIZE_LONG, REG_D(reg));
                    }
                    cycles = 4;
                }
            } else if ((opcode & 0x00C0) == 0x0000) {
                /* NBCD */
                /* TODO: implement BCD operations */
                goto illegal;
            } else if ((opcode & 0x01C0) == 0x0040) {
                /* PEA */
                uint32_t ea = m68k_get_ea(mode, reg, SIZE_LONG);
                g_cpu.a[7] -= 4;
                WRITE_LONG(g_cpu.a[7], ea);
                cycles = 12;
            }
            break;
            
        case 0xA:
            /* TST or TAS */
            if ((opcode & 0x00C0) == 0x00C0) {
                /* TAS */
                uint8_t val = m68k_read_ea(mode, reg, SIZE_BYTE);
                m68k_set_flags_logic(SIZE_BYTE, val);
                m68k_write_ea(mode, reg, SIZE_BYTE, val | 0x80);
                cycles = (mode == EA_MODE_DREG) ? 4 : 14;
            } else {
                /* TST */
                int size;
                switch ((opcode >> 6) & 0x3) {
                    case 0: size = SIZE_BYTE; break;
                    case 1: size = SIZE_WORD; break;
                    case 2: size = SIZE_LONG; break;
                    default: goto illegal;
                }
                uint32_t val = m68k_read_ea(mode, reg, size);
                m68k_set_flags_logic(size, val);
                cycles = 4;
            }
            break;
            
        case 0xE:
            if ((opcode & 0x0080) == 0) {
                /* TRAP, LINK, UNLK, MOVE USP, RESET, NOP, STOP, RTE, RTS, etc. */
                switch (opcode & 0x00FF) {
                    case 0x70: /* RESET */
                        if (!IS_SUPERVISOR()) {
                            m68k_exception(VEC_PRIVILEGE);
                            return 34;
                        }
                        /* Reset external devices (no-op in emulator) */
                        cycles = 132;
                        break;
                        
                    case 0x71: /* NOP */
                        cycles = 4;
                        break;
                        
                    case 0x72: /* STOP */
                        if (!IS_SUPERVISOR()) {
                            m68k_exception(VEC_PRIVILEGE);
                            return 34;
                        }
                        g_cpu.sr = FETCH_WORD();
                        g_cpu.stopped = 1;
                        cycles = 4;
                        break;
                        
                    case 0x73: /* RTE */
                        if (!IS_SUPERVISOR()) {
                            m68k_exception(VEC_PRIVILEGE);
                            return 34;
                        }
                        {
                            uint16_t new_sr = READ_WORD(g_cpu.a[7]);
                            g_cpu.a[7] += 2;
                            uint32_t new_pc = READ_LONG(g_cpu.a[7]);
                            g_cpu.a[7] += 4;
                            
                            g_cpu.sr = new_sr;
                            g_cpu.pc = new_pc;
                            
                            /* Check if switching to user mode */
                            if (!(new_sr & SR_S)) {
                                g_cpu.ssp = g_cpu.a[7];
                                g_cpu.a[7] = g_cpu.usp;
                            }
                        }
                        cycles = 20;
                        break;
                        
                    case 0x75: /* RTS */
                        g_cpu.pc = READ_LONG(g_cpu.a[7]);
                        g_cpu.a[7] += 4;
                        cycles = 16;
                        break;
                        
                    case 0x76: /* TRAPV */
                        if (g_cpu.sr & SR_V) {
                            m68k_exception(VEC_TRAPV);
                            return 34;
                        }
                        cycles = 4;
                        break;
                        
                    case 0x77: /* RTR */
                        {
                            uint16_t ccr = READ_WORD(g_cpu.a[7]) & 0xFF;
                            g_cpu.a[7] += 2;
                            g_cpu.sr = (g_cpu.sr & 0xFF00) | ccr;
                            g_cpu.pc = READ_LONG(g_cpu.a[7]);
                            g_cpu.a[7] += 4;
                        }
                        cycles = 20;
                        break;
                        
                    default:
                        if ((opcode & 0x00F0) == 0x0040) {
                            /* TRAP #n */
                            int vector = VEC_TRAP_BASE + (opcode & 0xF);
                            m68k_exception(vector);
                            return 34;
                        } else if ((opcode & 0x00F8) == 0x0050) {
                            /* LINK */
                            int areg = opcode & 0x7;
                            int16_t disp = (int16_t)FETCH_WORD();
                            g_cpu.a[7] -= 4;
                            WRITE_LONG(g_cpu.a[7], g_cpu.a[areg]);
                            g_cpu.a[areg] = g_cpu.a[7];
                            g_cpu.a[7] += disp;
                            cycles = 16;
                        } else if ((opcode & 0x00F8) == 0x0058) {
                            /* UNLK */
                            int areg = opcode & 0x7;
                            g_cpu.a[7] = g_cpu.a[areg];
                            g_cpu.a[areg] = READ_LONG(g_cpu.a[7]);
                            g_cpu.a[7] += 4;
                            cycles = 12;
                        } else if ((opcode & 0x00F8) == 0x0060) {
                            /* MOVE An,USP */
                            if (!IS_SUPERVISOR()) {
                                m68k_exception(VEC_PRIVILEGE);
                                return 34;
                            }
                            g_cpu.usp = g_cpu.a[opcode & 7];
                            cycles = 4;
                        } else if ((opcode & 0x00F8) == 0x0068) {
                            /* MOVE USP,An */
                            if (!IS_SUPERVISOR()) {
                                m68k_exception(VEC_PRIVILEGE);
                                return 34;
                            }
                            g_cpu.a[opcode & 7] = g_cpu.usp;
                            cycles = 4;
                        } else {
                            goto illegal;
                        }
                        break;
                }
            } else {
                /* JSR / JMP */
                uint32_t ea = m68k_get_ea(mode, reg, SIZE_LONG);
                
                if (opcode & 0x0040) {
                    /* JMP */
                    g_cpu.pc = ea;
                    cycles = 8;
                } else {
                    /* JSR */
                    g_cpu.a[7] -= 4;
                    WRITE_LONG(g_cpu.a[7], g_cpu.pc);
                    g_cpu.pc = ea;
                    cycles = 16;
                }
            }
            break;
            
        default:
            /* LEA - 0100 xxx 111 sss rrr */
            if ((opcode & 0x01C0) == 0x01C0) {
                int areg = OP_REG_DST(opcode);
                uint32_t ea = m68k_get_ea(mode, reg, SIZE_LONG);
                g_cpu.a[areg] = ea;
                cycles = 4;
            } else if ((opcode & 0x01C0) == 0x0180) {
                /* CHK */
                /* TODO: implement CHK */
                goto illegal;
            } else {
                goto illegal;
            }
            break;
    }
    
    return cycles;
    
illegal:
    m68k_exception(VEC_ILLEGAL_INST);
    return 34;
}

/*===========================================================================*/
/* Group 5: ADDQ/SUBQ/Scc/DBcc                                               */
/*===========================================================================*/

static int op_group_5(uint16_t opcode)
{
    int cycles = 4;
    int mode = OP_MODE_SRC(opcode);
    int reg = OP_REG_SRC(opcode);
    
    if ((opcode & 0x00C0) == 0x00C0) {
        /* Scc or DBcc */
        int cc = OP_COND(opcode);
        
        if (mode == 1) {
            /* DBcc */
            int16_t disp = (int16_t)FETCH_WORD();
            
            if (!eval_condition(cc)) {
                /* Condition false - decrement and branch */
                int16_t count = (int16_t)(REG_D(reg) & 0xFFFF) - 1;
                REG_D(reg) = (REG_D(reg) & 0xFFFF0000) | (uint16_t)count;
                
                if (count != -1) {
                    g_cpu.pc += disp - 2;  /* -2 because PC already advanced */
                    cycles = 10;
                } else {
                    cycles = 14;  /* Loop ended */
                }
            } else {
                cycles = 12;  /* Condition true */
            }
        } else {
            /* Scc */
            bool cond = eval_condition(cc);
            m68k_write_ea(mode, reg, SIZE_BYTE, cond ? 0xFF : 0x00);
            cycles = (mode == EA_MODE_DREG) ? 4 : 8;
            if (cond) cycles += 2;
        }
    } else {
        /* ADDQ or SUBQ */
        int size;
        switch ((opcode >> 6) & 0x3) {
            case 0: size = SIZE_BYTE; break;
            case 1: size = SIZE_WORD; break;
            case 2: size = SIZE_LONG; break;
            default: goto illegal;
        }
        
        int data = OP_REG_DST(opcode);
        if (data == 0) data = 8;  /* 0 means 8 */
        
        uint32_t val = m68k_read_ea(mode, reg, size);
        uint32_t res;
        
        if (opcode & 0x0100) {
            /* SUBQ */
            res = val - data;
            if (mode != EA_MODE_AREG) {
                m68k_set_flags_sub(size, data, val, res);
            }
        } else {
            /* ADDQ */
            res = val + data;
            if (mode != EA_MODE_AREG) {
                m68k_set_flags_add(size, data, val, res);
            }
        }
        
        m68k_write_ea(mode, reg, size, res);
        
        cycles = (size == SIZE_LONG) ? 8 : 4;
        if (mode != EA_MODE_DREG && mode != EA_MODE_AREG) {
            cycles += 4;
        }
    }
    
    return cycles;
    
illegal:
    m68k_exception(VEC_ILLEGAL_INST);
    return 34;
}

/*===========================================================================*/
/* Group 6: Bcc/BSR                                                          */
/*===========================================================================*/

static int op_group_6(uint16_t opcode)
{
    int cc = OP_COND(opcode);
    int8_t disp8 = (int8_t)(opcode & 0xFF);
    int32_t disp;
    int cycles;
    
    if (disp8 == 0) {
        /* 16-bit displacement */
        disp = (int16_t)FETCH_WORD();
        cycles = 10;
    } else if (disp8 == -1) {
        /* 32-bit displacement (68020+, illegal on 68000) */
        m68k_exception(VEC_ILLEGAL_INST);
        return 34;
    } else {
        disp = disp8;
        cycles = 10;
    }
    
    if (cc == 0) {
        /* BRA */
        g_cpu.pc += disp;
        cycles = 10;
    } else if (cc == 1) {
        /* BSR */
        g_cpu.a[7] -= 4;
        WRITE_LONG(g_cpu.a[7], g_cpu.pc);
        g_cpu.pc += disp;
        cycles = 18;
    } else {
        /* Bcc */
        if (eval_condition(cc)) {
            g_cpu.pc += disp;
            cycles = 10;
        } else {
            cycles = (disp8 == 0) ? 12 : 8;
        }
    }
    
    return cycles;
}

/*===========================================================================*/
/* Group 7: MOVEQ                                                            */
/*===========================================================================*/

static int op_group_7(uint16_t opcode)
{
    int reg = OP_REG_DST(opcode);
    int8_t data = (int8_t)(opcode & 0xFF);
    
    REG_D(reg) = SEXT8(data);
    m68k_set_flags_move(SIZE_LONG, REG_D(reg));
    
    return 4;
}

/*===========================================================================*/
/* Groups 8, 9, 11, 12, 13: Arithmetic/Logic                                 */
/*===========================================================================*/

static int op_arithmetic(uint16_t opcode)
{
    int group = OP_GROUP(opcode);
    int dreg = OP_REG_DST(opcode);
    int opmode = (opcode >> 6) & 0x7;
    int mode = OP_MODE_SRC(opcode);
    int reg = OP_REG_SRC(opcode);
    int cycles = 4;
    
    /* Determine size and direction */
    int size;
    bool to_reg;  /* true if result goes to Dn, false if to EA */
    
    switch (opmode) {
        case 0: size = SIZE_BYTE; to_reg = true; break;
        case 1: size = SIZE_WORD; to_reg = true; break;
        case 2: size = SIZE_LONG; to_reg = true; break;
        case 4: size = SIZE_BYTE; to_reg = false; break;
        case 5: size = SIZE_WORD; to_reg = false; break;
        case 6: size = SIZE_LONG; to_reg = false; break;
        case 3: /* DIVU/DIVS or MULU/MULS word size - special */
        case 7: /* DIVS or MULS long word - special */
            /* Handle MUL/DIV separately */
            if (group == 8 || group == 12) {
                goto mul_div;
            }
            /* For SUB/ADD with An */
            size = (opmode == 3) ? SIZE_WORD : SIZE_LONG;
            to_reg = true;
            mode = EA_MODE_AREG;  /* SUBA/ADDA */
            break;
        default:
            goto illegal;
    }
    
    uint32_t src, dst, res;
    
    if (to_reg) {
        src = m68k_read_ea(mode, reg, size);
        dst = REG_D(dreg);
    } else {
        src = REG_D(dreg);
        dst = m68k_read_ea(mode, reg, size);
    }
    
    switch (group) {
        case 8: /* OR */
            res = dst | src;
            m68k_set_flags_logic(size, res);
            break;
            
        case 9: /* SUB */
            res = dst - src;
            m68k_set_flags_sub(size, src, dst, res);
            break;
            
        case 11: /* CMP/EOR */
            if (to_reg) {
                /* CMP */
                res = dst - src;
                m68k_set_flags_sub(size, src, dst, res);
                return (size == SIZE_LONG) ? 6 : 4;  /* CMP doesn't write */
            } else {
                /* EOR */
                res = dst ^ src;
                m68k_set_flags_logic(size, res);
            }
            break;
            
        case 12: /* AND */
            res = dst & src;
            m68k_set_flags_logic(size, res);
            break;
            
        case 13: /* ADD */
            res = dst + src;
            m68k_set_flags_add(size, src, dst, res);
            break;
            
        default:
            goto illegal;
    }
    
    if (to_reg) {
        switch (size) {
            case SIZE_BYTE:
                REG_D(dreg) = (REG_D(dreg) & 0xFFFFFF00) | (res & 0xFF);
                break;
            case SIZE_WORD:
                REG_D(dreg) = (REG_D(dreg) & 0xFFFF0000) | (res & 0xFFFF);
                break;
            case SIZE_LONG:
                REG_D(dreg) = res;
                break;
        }
    } else {
        m68k_write_ea(mode, reg, size, res);
    }
    
    cycles = (size == SIZE_LONG) ? 8 : 4;
    if (!to_reg) cycles += 4;
    
    return cycles;
    
mul_div:
    /* MUL/DIV operations */
    if (group == 12) {
        /* MULU/MULS */
        uint16_t src16 = m68k_read_ea(mode, reg, SIZE_WORD);
        uint16_t dst16 = REG_D(dreg) & 0xFFFF;
        uint32_t result;
        
        if (opmode == 7) {
            /* MULS - signed */
            result = (int32_t)(int16_t)dst16 * (int32_t)(int16_t)src16;
        } else {
            /* MULU - unsigned */
            result = (uint32_t)dst16 * (uint32_t)src16;
        }
        
        REG_D(dreg) = result;
        m68k_set_flags_logic(SIZE_LONG, result);
        
        /* MUL takes 38-70 cycles depending on operand */
        return 70;
    } else if (group == 8) {
        /* DIVU/DIVS */
        uint16_t divisor = m68k_read_ea(mode, reg, SIZE_WORD);
        uint32_t dividend = REG_D(dreg);
        
        if (divisor == 0) {
            m68k_exception(VEC_ZERO_DIVIDE);
            return 38;
        }
        
        uint32_t quotient, remainder;
        
        if (opmode == 7) {
            /* DIVS - signed */
            int32_t sdividend = (int32_t)dividend;
            int16_t sdivisor = (int16_t)divisor;
            int32_t squotient = sdividend / sdivisor;
            int16_t sremainder = sdividend % sdivisor;
            
            /* Overflow if quotient doesn't fit in 16 bits */
            if (squotient > 32767 || squotient < -32768) {
                SET_FLAG(SR_V);
                return 140;
            }
            
            quotient = (uint16_t)squotient;
            remainder = (uint16_t)sremainder;
        } else {
            /* DIVU - unsigned */
            quotient = dividend / divisor;
            remainder = dividend % divisor;
            
            if (quotient > 65535) {
                SET_FLAG(SR_V);
                return 140;
            }
        }
        
        REG_D(dreg) = (remainder << 16) | (quotient & 0xFFFF);
        
        SET_FLAG_IF(SR_N, quotient & 0x8000);
        SET_FLAG_IF(SR_Z, (quotient & 0xFFFF) == 0);
        CLR_FLAG(SR_V);
        CLR_FLAG(SR_C);
        
        return 140;  /* DIV takes ~140 cycles */
    }
    
illegal:
    m68k_exception(VEC_ILLEGAL_INST);
    return 34;
}

/*===========================================================================*/
/* Group 14: Shift/Rotate                                                    */
/*===========================================================================*/

static int op_shift_rotate(uint16_t opcode)
{
    int cycles = 4;
    
    if ((opcode & 0x00C0) == 0x00C0) {
        /* Memory shift/rotate (word only) */
        int type = (opcode >> 9) & 0x3;
        int dir = (opcode >> 8) & 1;
        int mode = OP_MODE_SRC(opcode);
        int reg = OP_REG_SRC(opcode);
        
        uint16_t val = m68k_read_ea(mode, reg, SIZE_WORD);
        uint16_t res;
        bool carry;
        
        if (dir) {
            /* Left */
            carry = (val & 0x8000) != 0;
            res = val << 1;
            switch (type) {
                case 0: /* ASL */
                case 1: /* LSL */
                    break;
                case 2: /* ROXL */
                    res |= (g_cpu.sr & SR_X) ? 1 : 0;
                    break;
                case 3: /* ROL */
                    res |= carry ? 1 : 0;
                    break;
            }
        } else {
            /* Right */
            carry = (val & 1) != 0;
            if (type == 0) {
                /* ASR - arithmetic, preserve sign */
                res = (uint16_t)((int16_t)val >> 1);
            } else {
                res = val >> 1;
            }
            switch (type) {
                case 0: /* ASR */
                case 1: /* LSR */
                    break;
                case 2: /* ROXR */
                    res |= (g_cpu.sr & SR_X) ? 0x8000 : 0;
                    break;
                case 3: /* ROR */
                    res |= carry ? 0x8000 : 0;
                    break;
            }
        }
        
        SET_FLAG_IF(SR_C, carry);
        if (type != 3) SET_FLAG_IF(SR_X, carry);  /* ROL/ROR don't affect X */
        SET_FLAG_IF(SR_N, res & 0x8000);
        SET_FLAG_IF(SR_Z, res == 0);
        CLR_FLAG(SR_V);  /* V is complex for ASL, simplified here */
        
        m68k_write_ea(mode, reg, SIZE_WORD, res);
        cycles = 8;
    } else {
        /* Register shift/rotate */
        int count_reg = (opcode >> 9) & 7;
        int size;
        switch ((opcode >> 6) & 3) {
            case 0: size = SIZE_BYTE; break;
            case 1: size = SIZE_WORD; break;
            case 2: size = SIZE_LONG; break;
            default: goto illegal;
        }
        int type = (opcode >> 3) & 3;
        int dir = (opcode >> 8) & 1;
        int reg = opcode & 7;
        
        int count;
        if (opcode & 0x0020) {
            /* Count from register */
            count = REG_D(count_reg) & 63;
        } else {
            /* Immediate count (0 means 8) */
            count = count_reg;
            if (count == 0) count = 8;
        }
        
        uint32_t val = REG_D(reg);
        uint32_t mask, msb;
        switch (size) {
            case SIZE_BYTE: mask = 0xFF; msb = 0x80; break;
            case SIZE_WORD: mask = 0xFFFF; msb = 0x8000; break;
            default: mask = 0xFFFFFFFF; msb = 0x80000000; break;
        }
        val &= mask;
        
        uint32_t res = val;
        bool carry = false;
        bool x_flag = (g_cpu.sr & SR_X) != 0;
        
        for (int i = 0; i < count; i++) {
            if (dir) {
                /* Left */
                carry = (res & msb) != 0;
                res = (res << 1) & mask;
                switch (type) {
                    case 0: /* ASL */
                    case 1: /* LSL */
                        break;
                    case 2: /* ROXL */
                        res |= x_flag ? 1 : 0;
                        x_flag = carry;
                        break;
                    case 3: /* ROL */
                        res |= carry ? 1 : 0;
                        break;
                }
            } else {
                /* Right */
                carry = (res & 1) != 0;
                if (type == 0) {
                    /* ASR */
                    bool sign = (res & msb) != 0;
                    res = (res >> 1) & mask;
                    if (sign) res |= msb;
                } else {
                    res = (res >> 1) & mask;
                }
                switch (type) {
                    case 0: /* ASR */
                    case 1: /* LSR */
                        break;
                    case 2: /* ROXR */
                        res |= x_flag ? msb : 0;
                        x_flag = carry;
                        break;
                    case 3: /* ROR */
                        res |= carry ? msb : 0;
                        break;
                }
            }
        }
        
        if (count > 0) {
            SET_FLAG_IF(SR_C, carry);
            if (type != 3) SET_FLAG_IF(SR_X, carry);
        } else {
            CLR_FLAG(SR_C);
        }
        SET_FLAG_IF(SR_N, res & msb);
        SET_FLAG_IF(SR_Z, res == 0);
        CLR_FLAG(SR_V);
        
        /* Write back */
        REG_D(reg) = (REG_D(reg) & ~mask) | (res & mask);
        
        cycles = ((size == SIZE_LONG) ? 8 : 6) + count * 2;
    }
    
    return cycles;
    
illegal:
    m68k_exception(VEC_ILLEGAL_INST);
    return 34;
}

/*===========================================================================*/
/* Main Decoder                                                              */
/*===========================================================================*/

/**
 * @brief Decode and execute instruction
 * @param opcode 16-bit opcode
 * @return Cycles consumed
 */
int m68k_decode_execute(uint16_t opcode)
{
    int group = OP_GROUP(opcode);
    
    switch (group) {
        case 0x0:
            return op_group_0(opcode);
            
        case 0x1:
        case 0x2:
        case 0x3:
            return op_move(opcode);
            
        case 0x4:
            return op_group_4(opcode);
            
        case 0x5:
            return op_group_5(opcode);
            
        case 0x6:
            return op_group_6(opcode);
            
        case 0x7:
            return op_group_7(opcode);
            
        case 0x8:
        case 0x9:
        case 0xB:
        case 0xC:
        case 0xD:
            return op_arithmetic(opcode);
            
        case 0xA:
            /* Line A trap */
            m68k_exception(VEC_LINE_A);
            return 34;
            
        case 0xE:
            return op_shift_rotate(opcode);
            
        case 0xF:
            /* Line F trap */
            m68k_exception(VEC_LINE_F);
            return 34;
            
        default:
            m68k_exception(VEC_ILLEGAL_INST);
            return 34;
    }
}
