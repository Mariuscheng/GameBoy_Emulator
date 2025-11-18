// 指令解碼與執行相關實作，從 cpu.cpp 拆出
#include "cpu.h"
#include <iostream>
#include <unordered_map>
#include <functional>

// 指令解碼與執行相關實作，預留檔案（目前主要邏輯仍在 cpu.cpp、cpu_alu.cpp、cpu_cb.cpp、cpu_rotate.cpp、cpu_bits.cpp）。
// 若未來要進一步重構，建議把 cpu.cpp 的大 switch 搬移到這裡，並保持每個領域（ALU/CB/rotate/bits）在各自檔案內。

// 例如:
// void CPU::execute_instruction(uint8_t opcode) { ... }
// void CPU::execute_cb_instruction(uint8_t opcode) { ... }
// 其他 ALU/flag/bit 操作等

// Alternative implementation using std::unordered_map (for demonstration)
// This approach allows dynamic instruction registration but has slightly worse performance than switch-case
void CPU::execute_load_instructions_map(uint8_t opcode) {
    static std::unordered_map<uint8_t, std::function<void()>> load_instruction_map = {
        // 8-bit immediate loads (LD r, n)
        {0x06, [this]() { B = mmu.read_byte(PC++); }},
        {0x0E, [this]() { C = mmu.read_byte(PC++); }},
        {0x16, [this]() { D = mmu.read_byte(PC++); }},
        {0x1E, [this]() { E = mmu.read_byte(PC++); }},
        {0x26, [this]() { H = mmu.read_byte(PC++); }},
        {0x2E, [this]() { L = mmu.read_byte(PC++); }},
        {0x36, [this]() {
            uint8_t value = mmu.read_byte(PC++);
            if (timing_test_mode) {
                burn_tcycles(4);
                burn_tcycles(2);
                mmu.write_byte(HL, value);
                burn_tcycles(2);
            } else {
                mmu.write_byte(HL, value);
            }
        }},
        {0x3E, [this]() { A = mmu.read_byte(PC++); }},

        // 8-bit register loads (LD r, r) - showing first few as example
        {0x40, [this]() { B = B; }},
        {0x41, [this]() { B = C; }},
        {0x42, [this]() { B = D; }},
        // ... would continue for all register combinations

        // 16-bit loads
        {0x01, [this]() { C = mmu.read_byte(PC++); B = mmu.read_byte(PC++); }},
        {0x11, [this]() { E = mmu.read_byte(PC++); D = mmu.read_byte(PC++); }},
        {0x21, [this]() { L = mmu.read_byte(PC++); H = mmu.read_byte(PC++); }},
        {0x31, [this]() {
            uint8_t low = mmu.read_byte(PC++);
            uint8_t high = mmu.read_byte(PC++);
            SP = low | (high << 8);
        }},

        // LDH instructions
        {0xE0, [this]() {
            uint8_t offset = mmu.read_byte(PC++);
            if (timing_test_mode) {
                burn_tcycles(4);
                burn_tcycles(2);
                mmu.write_byte(0xFF00 + offset, A);
                burn_tcycles(2);
            } else {
                mmu.write_byte(0xFF00 + offset, A);
            }
        }},
        {0xF0, [this]() {
            uint8_t imm = mmu.read_byte(PC++);
            if (timing_test_mode) {
                burn_tcycles(4);
                A = mmu.read_byte(0xFF00 + imm);
                burn_tcycles(4);
            } else {
                A = mmu.read_byte(0xFF00 + imm);
            }
        }}
    };

    auto it = load_instruction_map.find(opcode);
    if (it != load_instruction_map.end()) {
        it->second(); // Execute the instruction
    } else {
        // Unknown load instruction - do nothing
    }
}

// Load instructions implementation (current switch-case approach)
void CPU::execute_load_instructions(uint8_t opcode) {
    switch (opcode) {
        // 8-bit immediate loads (LD r, n)
        case 0x06: // LD B, n
            B = mmu.read_byte(PC++);
            break;
        case 0x0E: // LD C, n
            C = mmu.read_byte(PC++);
            break;
        case 0x16: // LD D, n
            D = mmu.read_byte(PC++);
            break;
        case 0x1E: // LD E, n
            E = mmu.read_byte(PC++);
            break;
        case 0x26: // LD H, n
            H = mmu.read_byte(PC++);
            break;
        case 0x2E: // LD L, n
            L = mmu.read_byte(PC++);
            break;
        case 0x36: // LD (HL), n
            {
                uint8_t value = mmu.read_byte(PC++);
                if (timing_test_mode) {
                    burn_tcycles(4);             // M2 (immediate fetch) - T4-7
                    burn_tcycles(2);             // M3 setup - T8-9
                    mmu.write_byte(HL, value);   // Write at T10-11 (T2-3 of M3)
                    burn_tcycles(2);             // M3 complete - T10-11
                } else {
                    mmu.write_byte(HL, value);
                }
            }
            break;
        case 0x3E: // LD A, n
            A = mmu.read_byte(PC++);
            break;

        // 8-bit register loads (LD r, r)
        case 0x40: // LD B, B
            B = B;
            break;
        case 0x41: // LD B, C
            B = C;
            break;
        case 0x42: // LD B, D
            B = D;
            break;
        case 0x43: // LD B, E
            B = E;
            break;
        case 0x44: // LD B, H
            B = H;
            break;
        case 0x45: // LD B, L
            B = L;
            break;
        case 0x46: // LD B, (HL)
            if (timing_test_mode) {
                B = mmu.read_byte(HL);
                burn_tcycles(4);
            } else {
                B = mmu.read_byte(HL);
            }
            break;
        case 0x47: // LD B, A
            B = A;
            break;

        case 0x48: // LD C, B
            C = B;
            break;
        case 0x49: // LD C, C
            C = C;
            break;
        case 0x4A: // LD C, D
            C = D;
            break;
        case 0x4B: // LD C, E
            C = E;
            break;
        case 0x4C: // LD C, H
            C = H;
            break;
        case 0x4D: // LD C, L
            C = L;
            break;
        case 0x4E: // LD C, (HL)
            if (timing_test_mode) {
                C = mmu.read_byte(HL);
                burn_tcycles(4);
            } else {
                C = mmu.read_byte(HL);
            }
            break;
        case 0x4F: // LD C, A
            C = A;
            break;

        case 0x50: // LD D, B
            D = B;
            break;
        case 0x51: // LD D, C
            D = C;
            break;
        case 0x52: // LD D, D
            D = D;
            break;
        case 0x53: // LD D, E
            D = E;
            break;
        case 0x54: // LD D, H
            D = H;
            break;
        case 0x55: // LD D, L
            D = L;
            break;
        case 0x56: // LD D, (HL)
            if (timing_test_mode) {
                D = mmu.read_byte(HL);
                burn_tcycles(4);
            } else {
                D = mmu.read_byte(HL);
            }
            break;
        case 0x57: // LD D, A
            D = A;
            break;

        case 0x58: // LD E, B
            E = B;
            break;
        case 0x59: // LD E, C
            E = C;
            break;
        case 0x5A: // LD E, D
            E = D;
            break;
        case 0x5B: // LD E, E
            E = E;
            break;
        case 0x5C: // LD E, H
            E = H;
            break;
        case 0x5D: // LD E, L
            E = L;
            break;
        case 0x5E: // LD E, (HL)
            if (timing_test_mode) {
                E = mmu.read_byte(HL);
                burn_tcycles(4);
            } else {
                E = mmu.read_byte(HL);
            }
            break;
        case 0x5F: // LD E, A
            E = A;
            break;

        case 0x60: // LD H, B
            H = B;
            break;
        case 0x61: // LD H, C
            H = C;
            break;
        case 0x62: // LD H, D
            H = D;
            break;
        case 0x63: // LD H, E
            H = E;
            break;
        case 0x64: // LD H, H
            H = H;
            break;
        case 0x65: // LD H, L
            H = L;
            break;
        case 0x66: // LD H, (HL)
            if (timing_test_mode) {
                H = mmu.read_byte(HL);
                burn_tcycles(4);
            } else {
                H = mmu.read_byte(HL);
            }
            break;
        case 0x67: // LD H, A
            H = A;
            break;

        case 0x68: // LD L, B
            L = B;
            break;
        case 0x69: // LD L, C
            L = C;
            break;
        case 0x6A: // LD L, D
            L = D;
            break;
        case 0x6B: // LD L, E
            L = E;
            break;
        case 0x6C: // LD L, H
            L = H;
            break;
        case 0x6D: // LD L, L
            L = L;
            break;
        case 0x6E: // LD L, (HL)
            if (timing_test_mode) {
                L = mmu.read_byte(HL);
                burn_tcycles(4);
            } else {
                L = mmu.read_byte(HL);
            }
            break;
        case 0x6F: // LD L, A
            L = A;
            break;

        // LD (HL), r
        case 0x70: // LD (HL), B
            mmu.write_byte(HL, B);
            break;
        case 0x71: // LD (HL), C
            mmu.write_byte(HL, C);
            break;
        case 0x72: // LD (HL), D
            mmu.write_byte(HL, D);
            break;
        case 0x73: // LD (HL), E
            mmu.write_byte(HL, E);
            break;
        case 0x74: // LD (HL), H
            mmu.write_byte(HL, H);
            break;
        case 0x75: // LD (HL), L
            mmu.write_byte(HL, L);
            break;
        case 0x77: // LD (HL), A
            mmu.write_byte(HL, A);
            break;

        // LD A, r
        case 0x78: // LD A, B
            A = B;
            break;
        case 0x79: // LD A, C
            A = C;
            break;
        case 0x7A: // LD A, D
            A = D;
            break;
        case 0x7B: // LD A, E
            A = E;
            break;
        case 0x7C: // LD A, H
            A = H;
            break;
        case 0x7D: // LD A, L
            A = L;
            break;
        case 0x7E: // LD A, (HL)
            if (timing_test_mode) {
                A = mmu.read_byte(HL);
                burn_tcycles(4);
            } else {
                A = mmu.read_byte(HL);
            }
            break;
        case 0x7F: // LD A, A
            A = A;
            break;

        // 16-bit load instructions
        case 0x01: // LD BC, nn
            C = mmu.read_byte(PC++);
            B = mmu.read_byte(PC++);
            break;
        case 0x08: // LD (a16), SP
            {
                uint16_t addr = mmu.read_byte(PC++);
                addr |= mmu.read_byte(PC++) << 8;
                mmu.write_byte(addr, SP & 0xFF);
                mmu.write_byte(addr + 1, SP >> 8);
            }
            break;
        case 0x11: // LD DE, nn
            E = mmu.read_byte(PC++);
            D = mmu.read_byte(PC++);
            break;
        case 0x21: // LD HL, nn
            L = mmu.read_byte(PC++);
            H = mmu.read_byte(PC++);
            break;
        case 0x31: // LD SP, nn
            {
                uint8_t low = mmu.read_byte(PC++);
                uint8_t high = mmu.read_byte(PC++);
                SP = low | (high << 8);
                // Debug: Print SP value
                //std::cout << "DEBUG: LD SP, nn = 0x" << std::hex << SP << std::dec << std::endl;
            }
            break;

        // LD A, (rr)
        case 0x0A: // LD A, (BC)
            A = mmu.read_byte(BC);
            break;
        case 0x1A: // LD A, (DE)
            A = mmu.read_byte(DE);
            break;

        // LD (rr), A
        case 0x02: // LD (BC), A
            mmu.write_byte(BC, A);
            break;
        case 0x12: // LD (DE), A
            mmu.write_byte(DE, A);
            break;

        // LDH instructions (High RAM access)
        case 0xE0: // LDH (n), A
            {
                uint8_t offset = mmu.read_byte(PC++);
                if (timing_test_mode) {
                    burn_tcycles(4);                      // M2 (offset fetch) - T4-7
                    burn_tcycles(2);                      // M3 setup - T8-9
                    mmu.write_byte(0xFF00 + offset, A);   // Write at T10-11 (T2-3 of M3)
                    burn_tcycles(2);                      // M3 complete - T10-11
                } else {
                    mmu.write_byte(0xFF00 + offset, A);
                }
            }
            break;
        case 0xF0: // LDH A, (n)
            {
                uint8_t imm = mmu.read_byte(PC++);
                if (timing_test_mode) {
                    burn_tcycles(4);                 // M2 (imm fetch time represented; M1 burned in step())
                    A = mmu.read_byte(0xFF00 + imm); // M3 read at start
                    burn_tcycles(4);                 // finish M3
                } else {
                    A = mmu.read_byte(0xFF00 + imm);
                }
            }
            break;

        // LD A, (C) / LD (C), A
        case 0xE2: // LD (C), A
            mmu.write_byte(0xFF00 + C, A);
            break;
        case 0xF2: // LD A, (C)
            A = mmu.read_byte(0xFF00 + C);
            break;

        // LD A, (nn) / LD (nn), A
        case 0xEA: // LD (nn), A
            {
                uint16_t addr = mmu.read_byte(PC++);
                addr |= mmu.read_byte(PC++) << 8;
                if (timing_test_mode) {
                    burn_tcycles(4);             // M2 (addr low) - T4-7
                    burn_tcycles(4);             // M3 (addr high) - T8-11
                    burn_tcycles(2);             // M4 setup - T12-13
                    mmu.write_byte(addr, A);     // Write at T14-15 (T2-3 of M4)
                    burn_tcycles(2);             // M4 complete - T14-15
                } else {
                    mmu.write_byte(addr, A);
                }
            }
            break;
        case 0xFA: // LD A, (nn)
            {
                uint16_t addr = mmu.read_byte(PC++);
                addr |= mmu.read_byte(PC++) << 8;
                if (timing_test_mode) {
                    burn_tcycles(4);             // M2 (low)
                    burn_tcycles(4);             // M3 (high)
                    A = mmu.read_byte(addr);     // M4 read at start
                    burn_tcycles(4);             // finish M4
                } else {
                    A = mmu.read_byte(addr);
                }
            }
            break;

        // LD HL, SP+n
        case 0xF8: // LD HL, SP+n
            {
                int8_t e = (int8_t)mmu.read_byte(PC++);
                uint16_t sp = SP;
                uint16_t result = sp + e;
                if (log_file.is_open()) {
                    log_file << "LD HL,SP+e: SP=0x" << std::hex << sp
                             << " e=" << std::dec << (int)e
                             << " result=0x" << std::hex << result << std::dec << std::endl;
                }
                // Flags: Z=0, N=0, H from bit3 carry, C from bit7 carry of low-byte add (SP low + e)
                zero_flag = false;
                subtract_flag = false;
                half_carry_flag = ((sp & 0x0F) + ((uint8_t)e & 0x0F)) > 0x0F;
                carry_flag      = ((sp & 0xFF) + (uint8_t)e) > 0xFF;
                HL = result;
                if (log_file.is_open()) {
                    log_file << "    Flags: Z=" << zero_flag << " N=" << subtract_flag
                             << " H=" << half_carry_flag << " C=" << carry_flag << std::endl;
                }
            }
            break;
        case 0xF9: // LD SP, HL
            SP = HL;
            break;

        // LDI/LDD instructions
        case 0x22: // LDI (HL), A
            mmu.write_byte(HL, A);
            HL++;
            break;
        case 0x2A: // LDI A, (HL)
            A = mmu.read_byte(HL);
            HL++;
            break;
        case 0x32: // LDD (HL), A
            mmu.write_byte(HL, A);
            HL--;
            break;
        case 0x3A: // LDD A, (HL)
            A = mmu.read_byte(HL);
            HL--;
            break;

        default:
            // This should not happen if called correctly
            break;
    }
}
