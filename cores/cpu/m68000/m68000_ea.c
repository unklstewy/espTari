/**
 * @file m68000_ea.c
 * @brief MC68000 CPU Core - Effective Address Handling
 * 
 * Implements all 68000 effective address modes:
 * - Register direct (Dn, An)
 * - Register indirect (An), (An)+, -(An)
 * - Register indirect with displacement d16(An)
 * - Register indirect with index d8(An,Xn)
 * - Absolute short/long
 * - PC-relative with displacement d16(PC)
 * - PC-relative with index d8(PC,Xn)
 * - Immediate
 * 
 * SPDX-License-Identifier: MIT
 */

#include "m68000_internal.h"

/*===========================================================================*/
/* Effective Address Calculation                                             */
/*===========================================================================*/

/**
 * @brief Calculate effective address (does not read value)
 * @param mode Addressing mode (0-7)
 * @param reg Register number (0-7)
 * @param size Operand size (SIZE_BYTE, SIZE_WORD, SIZE_LONG)
 * @return Effective address (for modes that have one)
 * 
 * Note: For data/address register direct modes, returns a sentinel.
 * For true addressing modes, returns the computed address.
 */
uint32_t m68k_get_ea(int mode, int reg, int size)
{
    uint32_t ea = 0;
    
    switch (mode) {
        case EA_MODE_DREG:
            /* Data register direct - no address */
            return 0xFFFFFFFF;
            
        case EA_MODE_AREG:
            /* Address register direct - no address */
            return 0xFFFFFFFF;
            
        case EA_MODE_AREG_IND:
            /* (An) */
            ea = REG_A(reg);
            ADD_CYCLES(EA_TIME_AREG_IND);
            break;
            
        case EA_MODE_AREG_INC:
            /* (An)+ - address before increment */
            ea = REG_A(reg);
            ADD_CYCLES(EA_TIME_AREG_INC);
            /* Note: increment happens after read/write */
            break;
            
        case EA_MODE_AREG_DEC:
            /* -(An) - predecrement addressing */
            {
                int inc = (size == SIZE_BYTE && reg != 7) ? 1 : 
                          (size == SIZE_BYTE) ? 2 :  /* SP always word-aligned */
                          (size == SIZE_WORD) ? 2 : 4;
                ea = REG_A(reg) - inc;
                ADD_CYCLES(EA_TIME_AREG_DEC);
            }
            break;
            
        case EA_MODE_AREG_DISP:
            /* d16(An) */
            {
                int16_t disp = (int16_t)FETCH_WORD();
                ea = REG_A(reg) + disp;
                ADD_CYCLES(EA_TIME_AREG_DISP);
            }
            break;
            
        case EA_MODE_AREG_IDX:
            /* d8(An,Xn) */
            {
                uint16_t ext = FETCH_WORD();
                int8_t disp = (int8_t)(ext & 0xFF);
                int xn = (ext >> 12) & 7;
                bool is_addr = (ext & 0x8000) != 0;
                bool is_long = (ext & 0x0800) != 0;
                
                int32_t idx;
                if (is_addr) {
                    idx = is_long ? REG_A(xn) : SEXT16(REG_A(xn));
                } else {
                    idx = is_long ? REG_D(xn) : SEXT16(REG_D(xn));
                }
                
                ea = REG_A(reg) + disp + idx;
                ADD_CYCLES(EA_TIME_AREG_IDX);
            }
            break;
            
        case EA_MODE_OTHER:
            /* Extended addressing modes */
            switch (reg) {
                case EA_EXT_ABS_SHORT:
                    /* (xxx).W */
                    ea = SEXT16(FETCH_WORD());
                    ADD_CYCLES(EA_TIME_ABS_SHORT);
                    break;
                    
                case EA_EXT_ABS_LONG:
                    /* (xxx).L */
                    {
                        uint32_t hi = FETCH_WORD();
                        uint32_t lo = FETCH_WORD();
                        ea = (hi << 16) | lo;
                        ADD_CYCLES(EA_TIME_ABS_LONG);
                    }
                    break;
                    
                case EA_EXT_PC_DISP:
                    /* d16(PC) */
                    {
                        uint32_t pc = g_cpu.pc;  /* PC after extension word */
                        int16_t disp = (int16_t)FETCH_WORD();
                        ea = pc + disp;
                        ADD_CYCLES(EA_TIME_PC_DISP);
                    }
                    break;
                    
                case EA_EXT_PC_IDX:
                    /* d8(PC,Xn) */
                    {
                        uint32_t pc = g_cpu.pc;
                        uint16_t ext = FETCH_WORD();
                        int8_t disp = (int8_t)(ext & 0xFF);
                        int xn = (ext >> 12) & 7;
                        bool is_addr = (ext & 0x8000) != 0;
                        bool is_long = (ext & 0x0800) != 0;
                        
                        int32_t idx;
                        if (is_addr) {
                            idx = is_long ? REG_A(xn) : SEXT16(REG_A(xn));
                        } else {
                            idx = is_long ? REG_D(xn) : SEXT16(REG_D(xn));
                        }
                        
                        ea = pc + disp + idx;
                        ADD_CYCLES(EA_TIME_PC_IDX);
                    }
                    break;
                    
                case EA_EXT_IMMEDIATE:
                    /* #<data> - return address where immediate is */
                    ea = g_cpu.pc;
                    /* Don't add cycles here - done in read */
                    break;
                    
                default:
                    /* Invalid */
                    ea = 0;
                    break;
            }
            break;
            
        default:
            ea = 0;
            break;
    }
    
    return ea;
}

/**
 * @brief Read value from effective address
 * @param mode Addressing mode (0-7)
 * @param reg Register number (0-7)
 * @param size Operand size
 * @return Value at effective address
 */
uint32_t m68k_read_ea(int mode, int reg, int size)
{
    uint32_t value = 0;
    
    switch (mode) {
        case EA_MODE_DREG:
            /* Dn */
            switch (size) {
                case SIZE_BYTE: value = REG_D(reg) & 0xFF; break;
                case SIZE_WORD: value = REG_D(reg) & 0xFFFF; break;
                case SIZE_LONG: value = REG_D(reg); break;
            }
            break;
            
        case EA_MODE_AREG:
            /* An - word/long only (byte not allowed for An) */
            switch (size) {
                case SIZE_WORD: value = REG_A(reg) & 0xFFFF; break;
                case SIZE_LONG: value = REG_A(reg); break;
                default: value = REG_A(reg); break;
            }
            break;
            
        case EA_MODE_AREG_INC:
            /* (An)+ */
            {
                uint32_t ea = REG_A(reg);
                int inc = (size == SIZE_BYTE && reg != 7) ? 1 : 
                          (size == SIZE_BYTE) ? 2 :
                          (size == SIZE_WORD) ? 2 : 4;
                
                switch (size) {
                    case SIZE_BYTE: value = READ_BYTE(ea); break;
                    case SIZE_WORD: value = READ_WORD(ea); break;
                    case SIZE_LONG: value = READ_LONG(ea); break;
                }
                
                REG_A(reg) += inc;
                ADD_CYCLES(EA_TIME_AREG_INC);
            }
            break;
            
        case EA_MODE_AREG_DEC:
            /* -(An) */
            {
                int inc = (size == SIZE_BYTE && reg != 7) ? 1 : 
                          (size == SIZE_BYTE) ? 2 :
                          (size == SIZE_WORD) ? 2 : 4;
                REG_A(reg) -= inc;
                uint32_t ea = REG_A(reg);
                switch (size) {
                    case SIZE_BYTE: value = READ_BYTE(ea); break;
                    case SIZE_WORD: value = READ_WORD(ea); break;
                    case SIZE_LONG: value = READ_LONG(ea); break;
                }
                ADD_CYCLES(EA_TIME_AREG_DEC);
            }
            break;
            
        case EA_MODE_OTHER:
            if (reg == EA_EXT_IMMEDIATE) {
                /* Immediate - fetch from instruction stream */
                switch (size) {
                    case SIZE_BYTE:
                        value = FETCH_WORD() & 0xFF;
                        ADD_CYCLES(EA_TIME_IMMEDIATE);
                        break;
                    case SIZE_WORD:
                        value = FETCH_WORD();
                        ADD_CYCLES(EA_TIME_IMMEDIATE);
                        break;
                    case SIZE_LONG:
                        {
                            uint32_t hi = FETCH_WORD();
                            uint32_t lo = FETCH_WORD();
                            value = (hi << 16) | lo;
                            ADD_CYCLES(EA_TIME_IMMEDIATE + 4);
                        }
                        break;
                }
                break;
            }
            /* Fall through for other mode 7 cases */
            /* FALLTHROUGH */
            
        default:
            /* Memory addressing modes */
            {
                uint32_t ea = m68k_get_ea(mode, reg, size);
                switch (size) {
                    case SIZE_BYTE: value = READ_BYTE(ea); break;
                    case SIZE_WORD: value = READ_WORD(ea); break;
                    case SIZE_LONG: value = READ_LONG(ea); break;
                }
            }
            break;
    }
    
    return value;
}

/**
 * @brief Write value to effective address
 * @param mode Addressing mode (0-7)
 * @param reg Register number (0-7)
 * @param size Operand size
 * @param value Value to write
 */
void m68k_write_ea(int mode, int reg, int size, uint32_t value)
{
    switch (mode) {
        case EA_MODE_DREG:
            /* Dn */
            switch (size) {
                case SIZE_BYTE:
                    REG_D(reg) = (REG_D(reg) & 0xFFFFFF00) | (value & 0xFF);
                    break;
                case SIZE_WORD:
                    REG_D(reg) = (REG_D(reg) & 0xFFFF0000) | (value & 0xFFFF);
                    break;
                case SIZE_LONG:
                    REG_D(reg) = value;
                    break;
            }
            break;
            
        case EA_MODE_AREG:
            /* An - always writes full 32-bit (sign-extended for word) */
            switch (size) {
                case SIZE_WORD:
                    REG_A(reg) = SEXT16(value);
                    break;
                case SIZE_LONG:
                    REG_A(reg) = value;
                    break;
                default:
                    /* Byte not allowed for An */
                    REG_A(reg) = value;
                    break;
            }
            break;
            
        case EA_MODE_AREG_INC:
            /* (An)+ */
            {
                uint32_t ea = REG_A(reg);
                int inc = (size == SIZE_BYTE && reg != 7) ? 1 : 
                          (size == SIZE_BYTE) ? 2 :
                          (size == SIZE_WORD) ? 2 : 4;
                
                switch (size) {
                    case SIZE_BYTE: WRITE_BYTE(ea, value); break;
                    case SIZE_WORD: WRITE_WORD(ea, value); break;
                    case SIZE_LONG: WRITE_LONG(ea, value); break;
                }
                
                REG_A(reg) += inc;
                ADD_CYCLES(EA_TIME_AREG_INC);
            }
            break;
            
        case EA_MODE_AREG_DEC:
            /* -(An) */
            {
                int inc = (size == SIZE_BYTE && reg != 7) ? 1 : 
                          (size == SIZE_BYTE) ? 2 :
                          (size == SIZE_WORD) ? 2 : 4;
                REG_A(reg) -= inc;
                uint32_t ea = REG_A(reg);
                
                switch (size) {
                    case SIZE_BYTE: WRITE_BYTE(ea, value); break;
                    case SIZE_WORD: WRITE_WORD(ea, value); break;
                    case SIZE_LONG: WRITE_LONG(ea, value); break;
                }
                ADD_CYCLES(EA_TIME_AREG_DEC);
            }
            break;
            
        default:
            /* Memory addressing modes */
            {
                uint32_t ea = m68k_get_ea(mode, reg, size);
                switch (size) {
                    case SIZE_BYTE: WRITE_BYTE(ea, value); break;
                    case SIZE_WORD: WRITE_WORD(ea, value); break;
                    case SIZE_LONG: WRITE_LONG(ea, value); break;
                }
            }
            break;
    }
}
