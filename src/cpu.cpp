#include "cpu.h"
#include <iostream>
#include <fstream>

CPU::CPU(MMU& mmu) : mmu(mmu) {
    reset();
    log_file.open("cpu_log.txt");
}

CPU::~CPU() {
    if (log_file.is_open()) {
        log_file.close();
    }
}

void CPU::reset() {
    AF = 0x01B0;
    BC = 0x0013;
    DE = 0x00D8;
    HL = 0x014D;
    SP = 0xFFFE;
    PC = 0x0100;

    zero_flag = false;
    subtract_flag = false;
    half_carry_flag = false;
    carry_flag = false;

    ime = true; // Interrupts enabled by default
}

void CPU::step() {
    // Check for interrupts
    if (ime) {
        uint8_t ie_reg = mmu.read_byte(0xFFFF); // Interrupt Enable
        uint8_t if_reg = mmu.read_byte(0xFF0F); // Interrupt Flag

        uint8_t interrupts = ie_reg & if_reg;
        if (interrupts) {
            // Find highest priority interrupt
            for (int i = 0; i < 5; ++i) {
                if (interrupts & (1 << i)) {
                    handle_interrupt(i);
                    return; // Don't execute instruction this cycle
                }
            }
        }
    }

    uint8_t opcode = mmu.read_byte(PC++);
    execute_instruction(opcode);
}

void CPU::execute_instruction(uint8_t opcode) {
    // Log the instruction
    if (log_file.is_open()) {
        log_file << "PC: 0x" << std::hex << (PC - 1) << " Opcode: 0x" << (int)opcode << std::dec << std::endl;
    }

    // Handle CB prefix
    if (opcode == 0xCB) {
        uint8_t cb_opcode = mmu.read_byte(PC++);
        execute_cb_instruction(cb_opcode);
        return;
    }

    switch (opcode) {
        case 0x00: // NOP
            // No operation
            break;

        // 8-bit load instructions
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
            mmu.write_byte(HL, mmu.read_byte(PC++));
            break;
        case 0x3E: // LD A, n
            A = mmu.read_byte(PC++);
            break;
        case 0xE4: // Undefined/illegal opcode, treat as NOP
            break;

        // LD r1, r2 instructions
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
            B = mmu.read_byte(HL);
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
            C = mmu.read_byte(HL);
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
            D = mmu.read_byte(HL);
            break;
        case 0x57: // LD D, A
            D = A;
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
            H = mmu.read_byte(HL);
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
            L = mmu.read_byte(HL);
            break;
        case 0x6F: // LD L, A
            L = A;
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
            E = mmu.read_byte(HL);
            break;
        case 0x5F: // LD E, A
            E = A;
            break;

        // 16-bit load instructions
        case 0x01: // LD BC, nn
            C = mmu.read_byte(PC++);
            B = mmu.read_byte(PC++);
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
            SP = mmu.read_byte(PC++);
            SP |= mmu.read_byte(PC++) << 8;
            break;

        // ADD HL, rr
        case 0x09: // ADD HL, BC
            add_hl(BC);
            break;
        case 0x19: // ADD HL, DE
            add_hl(DE);
            break;
        case 0x29: // ADD HL, HL
            add_hl(HL);
            break;
        case 0x39: // ADD HL, SP
            add_hl(SP);
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
            mmu.write_byte(0xFF00 + mmu.read_byte(PC++), A);
            break;
        case 0xF0: // LDH A, (n)
            A = mmu.read_byte(0xFF00 + mmu.read_byte(PC++));
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
                mmu.write_byte(addr, A);
            }
            break;
        case 0xFA: // LD A, (nn)
            {
                uint16_t addr = mmu.read_byte(PC++);
                addr |= mmu.read_byte(PC++) << 8;
                A = mmu.read_byte(addr);
            }
            break;

        // LD HL, SP+n
        case 0xF8: // LD HL, SP+n
            {
                int8_t offset = (int8_t)mmu.read_byte(PC++);
                uint16_t result = SP + offset;
                zero_flag = false;
                subtract_flag = false;
                half_carry_flag = ((SP & 0x0F) + (offset & 0x0F)) > 0x0F;
                carry_flag = ((SP & 0xFF) + (offset & 0xFF)) > 0xFF;
                HL = result;
            }
            break;

        // ADD SP, n
        case 0xE8: // ADD SP, n
            {
                int8_t offset = (int8_t)mmu.read_byte(PC++);
                uint16_t result = SP + offset;
                zero_flag = false;
                subtract_flag = false;
                half_carry_flag = ((SP & 0x0F) + (offset & 0x0F)) > 0x0F;
                carry_flag = ((SP & 0xFF) + (offset & 0xFF)) > 0xFF;
                SP = result;
            }
            break;
        // Arithmetic instructions
        case 0x80: // ADD A, B
            add(B);
            break;
        case 0x81: // ADD A, C
            add(C);
            break;
        case 0x82: // ADD A, D
            add(D);
            break;
        case 0x83: // ADD A, E
            add(E);
            break;
        case 0x84: // ADD A, H
            add(H);
            break;
        case 0x85: // ADD A, L
            add(L);
            break;
        case 0x86: // ADD A, (HL)
            add(mmu.read_byte(HL));
            break;
        case 0x87: // ADD A, A
            add(A);
            break;

        // Arithmetic instructions (continued)
        case 0x90: // SUB A, B
            sub(B);
            break;
        case 0x91: // SUB A, C
            sub(C);
            break;
        case 0x92: // SUB A, D
            sub(D);
            break;
        case 0x93: // SUB A, E
            sub(E);
            break;
        case 0x94: // SUB A, H
            sub(H);
            break;
        case 0x95: // SUB A, L
            sub(L);
            break;
        case 0x96: // SUB A, (HL)
            sub(mmu.read_byte(HL));
            break;
        case 0x97: // SUB A, A
            sub(A);
            break;
        // SUB n
        case 0xD6: // SUB n
            sub(mmu.read_byte(PC++));
            break;

        // Logical instructions
        case 0xA0: // AND A, B
            and_op(B);
            break;
        case 0xA1: // AND A, C
            and_op(C);
            break;
        case 0xA2: // AND A, D
            and_op(D);
            break;
        case 0xA3: // AND A, E
            and_op(E);
            break;
        case 0xA4: // AND A, H
            and_op(H);
            break;
        case 0xA5: // AND A, L
            and_op(L);
            break;
        case 0xA6: // AND A, (HL)
            and_op(mmu.read_byte(HL));
            break;
        case 0xA7: // AND A, A
            and_op(A);
            break;

        // Logical instructions (continued)
        case 0xB0: // OR A, B
            or_op(B);
            break;
        case 0xB1: // OR A, C
            or_op(C);
            break;
        case 0xB2: // OR A, D
            or_op(D);
            break;
        case 0xB3: // OR A, E
            or_op(E);
            break;
        case 0xB4: // OR A, H
            or_op(H);
            break;
        case 0xB5: // OR A, L
            or_op(L);
            break;
        case 0xB6: // OR A, (HL)
            or_op(mmu.read_byte(HL));
            break;
        case 0xB7: // OR A, A
            or_op(A);
            break;

        // XOR instructions
        case 0xA8: // XOR A, B
            xor_op(B);
            break;
        case 0xA9: // XOR A, C
            xor_op(C);
            break;
        case 0xAA: // XOR A, D
            xor_op(D);
            break;
        case 0xAB: // XOR A, E
            xor_op(E);
            break;
        case 0xAC: // XOR A, H
            xor_op(H);
            break;
        case 0xAD: // XOR A, L
            xor_op(L);
            break;
        case 0xAE: // XOR A, (HL)
            xor_op(mmu.read_byte(HL));
            break;
        case 0xAF: // XOR A, A
            xor_op(A);
            break;

        // ADC instructions (Add with Carry)
        case 0x88: // ADC A, B
            adc(B);
            break;
        case 0x89: // ADC A, C
            adc(C);
            break;
        case 0x8A: // ADC A, D
            adc(D);
            break;
        case 0x8B: // ADC A, E
            adc(E);
            break;
        case 0x8C: // ADC A, H
            adc(H);
            break;
        case 0x8D: // ADC A, L
            adc(L);
            break;
        case 0x8E: // ADC A, (HL)
            adc(mmu.read_byte(HL));
            break;
        case 0x8F: // ADC A, A
            adc(A);
            break;
        // ADC n
        case 0xCE: // ADC A, n
            adc(mmu.read_byte(PC++));
            break;

        // SBC instructions (Subtract with Carry)
        case 0x98: // SBC A, B
            sbc(B);
            break;
        case 0x99: // SBC A, C
            sbc(C);
            break;
        case 0x9A: // SBC A, D
            sbc(D);
            break;
        case 0x9B: // SBC A, E
            sbc(E);
            break;
        case 0x9C: // SBC A, H
            sbc(H);
            break;
        case 0x9D: // SBC A, L
            sbc(L);
            break;
        case 0x9E: // SBC A, (HL)
            sbc(mmu.read_byte(HL));
            break;
        case 0x9F: // SBC A, A
            sbc(A);
            break;

        // Compare instructions
        case 0xB8: // CP A, B
            cp(B);
            break;
        case 0xB9: // CP A, C
            cp(C);
            break;
        case 0xBA: // CP A, D
            cp(D);
            break;
        case 0xBB: // CP A, E
            cp(E);
            break;
        case 0xBC: // CP A, H
            cp(H);
            break;
        case 0xBD: // CP A, L
            cp(L);
            break;
        case 0xBE: // CP A, (HL)
            cp(mmu.read_byte(HL));
            break;
        case 0xBF: // CP A, A
            cp(A);
            break;

        // Increment instructions
        case 0x04: // INC B
            inc(B);
            break;
        case 0x0C: // INC C
            inc(C);
            break;
        case 0x14: // INC D
            inc(D);
            break;
        case 0x1C: // INC E
            inc(E);
            break;
        case 0x24: // INC H
            inc(H);
            break;
        case 0x2C: // INC L
            inc(L);
            break;
        case 0x34: // INC (HL)
            {
                uint8_t value = mmu.read_byte(HL);
                inc(value);
                mmu.write_byte(HL, value);
            }
            break;
        case 0x3C: // INC A
            inc(A);
            break;

        // Decrement instructions
        case 0x05: // DEC B
            dec(B);
            break;
        case 0x0D: // DEC C
            dec(C);
            break;
        case 0x15: // DEC D
            dec(D);
            break;
        case 0x1D: // DEC E
            dec(E);
            break;
        case 0x25: // DEC H
            dec(H);
            break;
        case 0x2D: // DEC L
            dec(L);
            break;
        case 0x35: // DEC (HL)
            {
                uint8_t value = mmu.read_byte(HL);
                dec(value);
                mmu.write_byte(HL, value);
            }
            break;
        case 0x3D: // DEC A
            dec(A);
            break;

        // 16-bit increment
        case 0x03: // INC BC
            BC++;
            break;
        case 0x13: // INC DE
            DE++;
            break;
        case 0x23: // INC HL
            HL++;
            break;
        case 0x33: // INC SP
            SP++;
            break;

        // 16-bit decrement
        case 0x0B: // DEC BC
            BC--;
            break;
        case 0x1B: // DEC DE
            DE--;
            break;
        case 0x2B: // DEC HL
            HL--;
            break;
        case 0x3B: // DEC SP
            SP--;
            break;

        // Stack operations
        case 0xC1: // POP BC
            C = mmu.read_byte(SP++);
            B = mmu.read_byte(SP++);
            break;
        case 0xD1: // POP DE
            E = mmu.read_byte(SP++);
            D = mmu.read_byte(SP++);
            break;
        case 0xE1: // POP HL
            L = mmu.read_byte(SP++);
            H = mmu.read_byte(SP++);
            break;
        case 0xF1: // POP AF
            F = mmu.read_byte(SP++);
            A = mmu.read_byte(SP++);
            break;

        case 0xC5: // PUSH BC
            SP -= 2;
            mmu.write_byte(SP, C);
            mmu.write_byte(SP + 1, B);
            break;
        case 0xD5: // PUSH DE
            SP -= 2;
            mmu.write_byte(SP, E);
            mmu.write_byte(SP + 1, D);
            break;
        case 0xE5: // PUSH HL
            SP -= 2;
            mmu.write_byte(SP, L);
            mmu.write_byte(SP + 1, H);
            break;
        case 0xF5: // PUSH AF
            SP -= 2;
            mmu.write_byte(SP, F);
            mmu.write_byte(SP + 1, A);
            break;

        // Jump instructions
        case 0xC3: // JP nn
            {
                uint16_t addr = mmu.read_byte(PC) | (mmu.read_byte(PC + 1) << 8);
                PC = addr;
            }
            break;
        case 0xC2: // JP NZ, nn
            if (!zero_flag) {
                uint16_t addr = mmu.read_byte(PC) | (mmu.read_byte(PC + 1) << 8);
                PC = addr;
            } else {
                PC += 2;
            }
            break;
        case 0xCA: // JP Z, nn
            if (zero_flag) {
                uint16_t addr = mmu.read_byte(PC) | (mmu.read_byte(PC + 1) << 8);
                PC = addr;
            } else {
                PC += 2;
            }
            break;
        case 0xD2: // JP NC, nn
            if (!carry_flag) {
                uint16_t addr = mmu.read_byte(PC) | (mmu.read_byte(PC + 1) << 8);
                PC = addr;
            } else {
                PC += 2;
            }
            break;
        case 0xDA: // JP C, nn
            if (carry_flag) {
                uint16_t addr = mmu.read_byte(PC) | (mmu.read_byte(PC + 1) << 8);
                PC = addr;
            } else {
                PC += 2;
            }
            break;
        case 0xE9: // JP (HL)
            PC = HL;
            break;

        // Jump relative
        case 0x18: // JR n
            PC += (int8_t)mmu.read_byte(PC++);
            break;
        case 0x20: // JR NZ, n
            if (!zero_flag) {
                PC += (int8_t)mmu.read_byte(PC++);
            } else {
                PC++;
            }
            break;
        case 0x28: // JR Z, n
            if (zero_flag) {
                PC += (int8_t)mmu.read_byte(PC++);
            } else {
                PC++;
            }
            break;
        case 0x30: // JR NC, n
            if (!carry_flag) {
                PC += (int8_t)mmu.read_byte(PC++);
            } else {
                PC++;
            }
            break;
        case 0x38: // JR C, n
            if (carry_flag) {
                PC += (int8_t)mmu.read_byte(PC++);
            } else {
                PC++;
            }
            break;

        // Interrupt control
        case 0xF3: // DI (Disable Interrupts)
            ime = false;
            break;
        case 0xFB: // EI (Enable Interrupts)
            ime = true;
            break;

        // Return from interrupt
        case 0xD9: // RETI
            PC = mmu.read_byte(SP) | (mmu.read_byte(SP + 1) << 8);
            SP += 2;
            ime = true; // Re-enable interrupts
            break;

        // Call instructions
        case 0xCD: // CALL nn
            {
                uint16_t addr = mmu.read_byte(PC) | (mmu.read_byte(PC + 1) << 8);
                PC += 2;
                SP -= 2;
                mmu.write_byte(SP, PC & 0xFF);
                mmu.write_byte(SP + 1, PC >> 8);
                PC = addr;
            }
            break;

        // Return instructions
        case 0xC9: // RET
            PC = mmu.read_byte(SP) | (mmu.read_byte(SP + 1) << 8);
            SP += 2;
            break;

        // Rotate and shift instructions
        case 0x07: // RLCA (Rotate Left Circular Accumulator)
            rlca();
            break;
        case 0x0F: // RRCA (Rotate Right Circular Accumulator)
            rrca();
            break;
        case 0x17: // RLA (Rotate Left Accumulator through Carry)
            rla();
            break;
        case 0x1F: // RRA (Rotate Right Accumulator through Carry)
            rra();
            break;

        // CPL (Complement A)
        case 0x2F: // CPL
            A = ~A;
            subtract_flag = true;
            half_carry_flag = true;
            break;

        // SCF (Set Carry Flag)
        case 0x37: // SCF
            carry_flag = true;
            subtract_flag = false;
            half_carry_flag = false;
            break;

        // CCF (Complement Carry Flag)
        case 0x3F: // CCF
            carry_flag = !carry_flag;
            subtract_flag = false;
            half_carry_flag = false;
            break;

        // HALT
        case 0x76: // HALT
            // TODO: Implement HALT (stop CPU until interrupt)
            break;

        // RST instructions (Restart)
        case 0xC7: // RST 00H
            rst(0x00);
            break;
        case 0xCF: // RST 08H
            rst(0x08);
            break;
        case 0xD7: // RST 10H
            rst(0x10);
            break;
        case 0xDF: // RST 18H
            rst(0x18);
            break;
        case 0xE7: // RST 20H
            rst(0x20);
            break;
        case 0xEF: // RST 28H
            rst(0x28);
            break;
        case 0xF7: // RST 30H
            rst(0x30);
            break;
        case 0xFF: // RST 38H
            rst(0x38);
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

        // LD A, r2
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
            A = mmu.read_byte(HL);
            break;
        case 0x7F: // LD A, A
            A = A;
            break;

        // AND n
        case 0xE6: // AND n
            and_op(mmu.read_byte(PC++));
            break;

        // CALL conditional
        case 0xC4: // CALL NZ, nn
            if (!zero_flag) {
                uint16_t addr = mmu.read_byte(PC) | (mmu.read_byte(PC + 1) << 8);
                PC += 2;
                SP -= 2;
                mmu.write_byte(SP, PC & 0xFF);
                mmu.write_byte(SP + 1, PC >> 8);
                PC = addr;
            } else {
                PC += 2;
            }
            break;
        case 0xCC: // CALL Z, nn
            if (zero_flag) {
                uint16_t addr = mmu.read_byte(PC) | (mmu.read_byte(PC + 1) << 8);
                PC += 2;
                SP -= 2;
                mmu.write_byte(SP, PC & 0xFF);
                mmu.write_byte(SP + 1, PC >> 8);
                PC = addr;
            } else {
                PC += 2;
            }
            break;
        case 0xD4: // CALL NC, nn
            if (!carry_flag) {
                uint16_t addr = mmu.read_byte(PC) | (mmu.read_byte(PC + 1) << 8);
                PC += 2;
                SP -= 2;
                mmu.write_byte(SP, PC & 0xFF);
                mmu.write_byte(SP + 1, PC >> 8);
                PC = addr;
            } else {
                PC += 2;
            }
            break;
        case 0xDC: // CALL C, nn
            if (carry_flag) {
                uint16_t addr = mmu.read_byte(PC) | (mmu.read_byte(PC + 1) << 8);
                PC += 2;
                SP -= 2;
                mmu.write_byte(SP, PC & 0xFF);
                mmu.write_byte(SP + 1, PC >> 8);
                PC = addr;
            } else {
                PC += 2;
            }
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

        // CP n
        case 0xFE: // CP n
            cp(mmu.read_byte(PC++));
            break;

        // RET conditional
        case 0xC0: // RET NZ
            if (!zero_flag) {
                PC = mmu.read_byte(SP) | (mmu.read_byte(SP + 1) << 8);
                SP += 2;
            }
            break;
        case 0xC8: // RET Z
            if (zero_flag) {
                PC = mmu.read_byte(SP) | (mmu.read_byte(SP + 1) << 8);
                SP += 2;
            }
            break;
        case 0xD0: // RET NC
            if (!carry_flag) {
                PC = mmu.read_byte(SP) | (mmu.read_byte(SP + 1) << 8);
                SP += 2;
            }
            break;
        case 0xD8: // RET C
            if (carry_flag) {
                PC = mmu.read_byte(SP) | (mmu.read_byte(SP + 1) << 8);
                SP += 2;
            }
            break;

        // Instruction not implemented yet
        default:
            if (log_file.is_open()) {
                log_file << "Instruction not implemented yet: 0x" << std::hex << (int)opcode << std::dec << std::endl;
            }
            std::cout << "Instruction not implemented yet: 0x" << std::hex << (int)opcode << std::dec << std::endl;
            break;
    }
}

void CPU::handle_interrupt(uint8_t interrupt_type) {
    if (!ime) return;

    ime = false; // Disable interrupts

    // Push PC to stack
    SP -= 2;
    mmu.write_byte(SP, PC & 0xFF);
    mmu.write_byte(SP + 1, PC >> 8);

    // Jump to interrupt vector
    switch (interrupt_type) {
        case 0: PC = 0x40; break; // VBlank
        case 1: PC = 0x48; break; // LCD
        case 2: PC = 0x50; break; // Timer
        case 3: PC = 0x58; break; // Serial
        case 4: PC = 0x60; break; // Joypad
    }

    // Clear interrupt flag
    uint8_t if_reg = mmu.read_byte(0xFF0F);
    if_reg &= ~(1 << interrupt_type);
    mmu.write_byte(0xFF0F, if_reg);
}

void CPU::add(uint8_t value) {
    uint16_t result = A + value;
    zero_flag = (result & 0xFF) == 0;
    subtract_flag = false;
    half_carry_flag = ((A & 0x0F) + (value & 0x0F)) > 0x0F;
    carry_flag = result > 0xFF;
    A = result & 0xFF;
}

void CPU::sub(uint8_t value) {
    uint16_t result = A - value;
    zero_flag = (result & 0xFF) == 0;
    subtract_flag = true;
    half_carry_flag = (A & 0x0F) < (value & 0x0F);
    carry_flag = A < value;
    A = result & 0xFF;
}

void CPU::and_op(uint8_t value) {
    A &= value;
    zero_flag = A == 0;
    subtract_flag = false;
    half_carry_flag = true;
    carry_flag = false;
}

void CPU::or_op(uint8_t value) {
    A |= value;
    zero_flag = A == 0;
    subtract_flag = false;
    half_carry_flag = false;
    carry_flag = false;
}

void CPU::xor_op(uint8_t value) {
    A ^= value;
    zero_flag = A == 0;
    subtract_flag = false;
    half_carry_flag = false;
    carry_flag = false;
}

void CPU::adc(uint8_t value) {
    uint16_t carry = carry_flag ? 1 : 0;
    uint16_t result = A + value + carry;
    zero_flag = (result & 0xFF) == 0;
    subtract_flag = false;
    half_carry_flag = ((A & 0x0F) + (value & 0x0F) + carry) > 0x0F;
    carry_flag = result > 0xFF;
    A = result & 0xFF;
}

void CPU::sbc(uint8_t value) {
    uint16_t carry = carry_flag ? 1 : 0;
    uint16_t result = A - value - carry;
    zero_flag = (result & 0xFF) == 0;
    subtract_flag = true;
    half_carry_flag = (A & 0x0F) < ((value & 0x0F) + carry);
    carry_flag = A < (value + carry);
    A = result & 0xFF;
}

void CPU::cp(uint8_t value) {
    uint16_t result = A - value;
    zero_flag = (result & 0xFF) == 0;
    subtract_flag = true;
    half_carry_flag = (A & 0x0F) < (value & 0x0F);
    carry_flag = A < value;
    // A is not modified
}

void CPU::inc(uint8_t& reg) {
    reg++;
    zero_flag = reg == 0;
    subtract_flag = false;
    half_carry_flag = (reg & 0x0F) == 0;
}

void CPU::dec(uint8_t& reg) {
    reg--;
    zero_flag = reg == 0;
    subtract_flag = true;
    half_carry_flag = (reg & 0x0F) == 0x0F;
}

void CPU::rlca() {
    carry_flag = (A & 0x80) != 0;
    A = (A << 1) | (carry_flag ? 1 : 0);
    zero_flag = false;
    subtract_flag = false;
    half_carry_flag = false;
}

void CPU::rrca() {
    carry_flag = (A & 0x01) != 0;
    A = (A >> 1) | (carry_flag ? 0x80 : 0);
    zero_flag = false;
    subtract_flag = false;
    half_carry_flag = false;
}

void CPU::rla() {
    bool old_carry = carry_flag;
    carry_flag = (A & 0x80) != 0;
    A = (A << 1) | (old_carry ? 1 : 0);
    zero_flag = false;
    subtract_flag = false;
    half_carry_flag = false;
}

void CPU::rra() {
    bool old_carry = carry_flag;
    carry_flag = (A & 0x01) != 0;
    A = (A >> 1) | (old_carry ? 0x80 : 0);
    zero_flag = false;
    subtract_flag = false;
    half_carry_flag = false;
}

void CPU::rst(uint8_t addr) {
    SP -= 2;
    mmu.write_byte(SP, PC & 0xFF);
    mmu.write_byte(SP + 1, PC >> 8);
    PC = addr;
}

void CPU::add_hl(uint16_t value) {
    uint32_t result = HL + value;
    subtract_flag = false;
    half_carry_flag = ((HL & 0x0FFF) + (value & 0x0FFF)) > 0x0FFF;
    carry_flag = result > 0xFFFF;
    HL = result & 0xFFFF;
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

uint8_t& CPU::get_register_ref(uint8_t reg_code) {
    switch (reg_code) {
        case 0: return B;
        case 1: return C;
        case 2: return D;
        case 3: return E;
        case 4: return H;
        case 5: return L;
        case 7: return A;
        default: return A; // Error, but 6 is (HL) handled separately
    }
}

void CPU::rlc(uint8_t& reg) {
    carry_flag = (reg & 0x80) != 0;
    reg = (reg << 1) | (carry_flag ? 1 : 0);
    zero_flag = reg == 0;
    subtract_flag = false;
    half_carry_flag = false;
}

void CPU::rrc(uint8_t& reg) {
    carry_flag = (reg & 0x01) != 0;
    reg = (reg >> 1) | (carry_flag ? 0x80 : 0);
    zero_flag = reg == 0;
    subtract_flag = false;
    half_carry_flag = false;
}

void CPU::rl(uint8_t& reg) {
    bool old_carry = carry_flag;
    carry_flag = (reg & 0x80) != 0;
    reg = (reg << 1) | (old_carry ? 1 : 0);
    zero_flag = reg == 0;
    subtract_flag = false;
    half_carry_flag = false;
}

void CPU::rr(uint8_t& reg) {
    bool old_carry = carry_flag;
    carry_flag = (reg & 0x01) != 0;
    reg = (reg >> 1) | (old_carry ? 0x80 : 0);
    zero_flag = reg == 0;
    subtract_flag = false;
    half_carry_flag = false;
}

void CPU::sla(uint8_t& reg) {
    carry_flag = (reg & 0x80) != 0;
    reg <<= 1;
    zero_flag = reg == 0;
    subtract_flag = false;
    half_carry_flag = false;
}

void CPU::sra(uint8_t& reg) {
    carry_flag = (reg & 0x01) != 0;
    reg = (reg >> 1) | (reg & 0x80);
    zero_flag = reg == 0;
    subtract_flag = false;
    half_carry_flag = false;
}

void CPU::swap(uint8_t& reg) {
    reg = ((reg & 0x0F) << 4) | ((reg & 0xF0) >> 4);
    zero_flag = reg == 0;
    subtract_flag = false;
    half_carry_flag = false;
    carry_flag = false;
}

void CPU::srl(uint8_t& reg) {
    carry_flag = (reg & 0x01) != 0;
    reg >>= 1;
    zero_flag = reg == 0;
    subtract_flag = false;
    half_carry_flag = false;
}

void CPU::bit(uint8_t bit, uint8_t value) {
    zero_flag = (value & (1 << bit)) == 0;
    subtract_flag = false;
    half_carry_flag = true;
}

void CPU::res(uint8_t bit, uint8_t& reg) {
    reg &= ~(1 << bit);
}

void CPU::set(uint8_t bit, uint8_t& reg) {
    reg |= (1 << bit);
}
