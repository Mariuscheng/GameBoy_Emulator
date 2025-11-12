#include "cpu.h"
#include <iostream>

uint8_t& CPU::get_register_ref(uint8_t reg_code) {
    switch (reg_code) {
        case 0: return B;
        case 1: return C;
        case 2: return D;
        case 3: return E;
        case 4: return H;
        case 5: return L;
        case 6: 
            // (HL) should be handled separately before calling this function
            // Return dummy reference to avoid undefined behavior
            std::cerr << "ERROR: get_register_ref called with reg_code 6 (HL), which should be handled separately!" << std::endl;
            return A; // Fallback, but this indicates a serious bug
        case 7: return A;
        default: 
            std::cerr << "ERROR: get_register_ref called with invalid reg_code: " << (int)reg_code << std::endl;
            return A; // Fallback
    }
}

void CPU::execute_cb_instruction(uint8_t cb_opcode) {
    uint8_t reg_code = cb_opcode & 0x07;
    uint8_t operation = cb_opcode >> 3;

    if (reg_code == 6) { // (HL)
        uint8_t value = mmu.read_byte(HL);
        uint8_t result;

        switch (operation) {
            case 0: // RLC (HL)
                carry_flag = (value & 0x80) != 0;
                result = (value << 1) | (carry_flag ? 1 : 0);
                zero_flag = result == 0;
                subtract_flag = false;
                half_carry_flag = false;
                mmu.write_byte(HL, result);
                break;
            case 1: // RRC (HL)
                carry_flag = (value & 0x01) != 0;
                result = (value >> 1) | (carry_flag ? 0x80 : 0);
                zero_flag = result == 0;
                subtract_flag = false;
                half_carry_flag = false;
                mmu.write_byte(HL, result);
                break;
            case 2: // RL (HL)
                {
                    bool old_carry = carry_flag;
                    carry_flag = (value & 0x80) != 0;
                    result = (value << 1) | (old_carry ? 1 : 0);
                    zero_flag = result == 0;
                    subtract_flag = false;
                    half_carry_flag = false;
                    mmu.write_byte(HL, result);
                }
                break;
            case 3: // RR (HL)
                {
                    bool old_carry = carry_flag;
                    carry_flag = (value & 0x01) != 0;
                    result = (value >> 1) | (old_carry ? 0x80 : 0);
                    zero_flag = result == 0;
                    subtract_flag = false;
                    half_carry_flag = false;
                    mmu.write_byte(HL, result);
                }
                break;
            case 4: // SLA (HL)
                carry_flag = (value & 0x80) != 0;
                result = value << 1;
                zero_flag = result == 0;
                subtract_flag = false;
                half_carry_flag = false;
                mmu.write_byte(HL, result);
                break;
            case 5: // SRA (HL)
                carry_flag = (value & 0x01) != 0;
                result = (value >> 1) | (value & 0x80);
                zero_flag = result == 0;
                subtract_flag = false;
                half_carry_flag = false;
                mmu.write_byte(HL, result);
                break;
            case 6: // SWAP (HL)
                result = ((value & 0x0F) << 4) | ((value & 0xF0) >> 4);
                zero_flag = result == 0;
                subtract_flag = false;
                half_carry_flag = false;
                carry_flag = false;
                mmu.write_byte(HL, result);
                break;
            case 7: // SRL (HL)
                carry_flag = (value & 0x01) != 0;
                result = value >> 1;
                zero_flag = result == 0;
                subtract_flag = false;
                half_carry_flag = false;
                mmu.write_byte(HL, result);
                break;
            default:
                if (operation >= 8 && operation <= 15) { // BIT
                    uint8_t bit = operation - 8;
                    zero_flag = (value & (1 << bit)) == 0;
                    subtract_flag = false;
                    half_carry_flag = true;
                    // carry_flag remains unchanged
                } else if (operation >= 16 && operation <= 23) { // RES
                    uint8_t bit = operation - 16;
                    result = value & ~(1 << bit);
                    mmu.write_byte(HL, result);
                } else if (operation >= 24 && operation <= 31) { // SET
                    uint8_t bit = operation - 24;
                    result = value | (1 << bit);
                    mmu.write_byte(HL, result);
                }
                break;
        }
    } else { // Regular registers
        uint8_t& reg = get_register_ref(reg_code);

        switch (operation) {
            case 0: rlc(reg); break;
            case 1: rrc(reg); break;
            case 2: rl(reg); break;
            case 3: rr(reg); break;
            case 4: sla(reg); break;
            case 5: sra(reg); break;
            case 6: swap(reg); break;
            case 7: srl(reg); break;
            default:
                if (operation >= 8 && operation <= 15) { // BIT
                    uint8_t bit_val = operation - 8;
                    bit(bit_val, reg);
                } else if (operation >= 16 && operation <= 23) { // RES
                    uint8_t bit_val = operation - 16;
                    res(bit_val, reg);
                } else if (operation >= 24 && operation <= 31) { // SET
                    uint8_t bit_val = operation - 24;
                    set(bit_val, reg);
                }
                break;
        }
    }
}
