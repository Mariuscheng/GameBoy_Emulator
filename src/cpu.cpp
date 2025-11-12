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
    AF = 0x01B0;  // Initial AF: A=0x01, F=0xB0 (Z=1, N=0, H=1, C=0)
    BC = 0x0013;
    DE = 0x00D8;
    HL = 0x014D;
    SP = 0xFFFE;
    PC = 0x0100;

    // Load flags from F register (set by AF = 0x01B0)
    load_flags_from_f();

    ime = false; // Interrupts DISABLED by default on startup
    halted = false;
    ei_delay_pending = false; // No EI delay pending on reset
}

int CPU::step() {
    // Check for interrupts
    if (ime) {
        uint8_t ie_reg = mmu.read_byte(0xFFFF); // Interrupt Enable
        uint8_t if_reg = mmu.read_byte(0xFF0F); // Interrupt Flag

        uint8_t interrupts = ie_reg & if_reg;
        if (interrupts) {
            halted = false; // Wake up from HALT
            // Find highest priority interrupt
            for (int i = 0; i < 5; ++i) {
                if (interrupts & (1 << i)) {
                    handle_interrupt(i);
                    // Update timer with interrupt handling cycles
                    mmu.update_timer_cycles(20);
                    return 20; // Interrupt handling takes 20 cycles
                }
            }
        }
    }

    // If CPU is halted and no interrupts, don't execute instructions
    if (halted) {
        // Update timer even in HALT mode
        mmu.update_timer_cycles(4);
        return 4; // HALT consumes 4 cycles
    }

    uint8_t opcode = mmu.read_byte(PC++);
    int cycles = execute_instruction_with_cycles(opcode);

    // Update timer based on instruction cycles
    mmu.update_timer_cycles(static_cast<uint8_t>(cycles));

    // Apply EI delay - EI takes effect after the next instruction executes
    if (ei_delay_pending) {
        ime = true;
        ei_delay_pending = false;
    }

    return cycles;
}

void CPU::execute_instruction(uint8_t opcode) {
    // Logging is done in execute_instruction_with_cycles, not here
    // to avoid duplicate logging

    // Handle CB prefix
    if (opcode == 0xCB) {
        uint8_t cb_opcode = mmu.read_byte(PC++);
        execute_cb_instruction(cb_opcode);
        sync_f_register(); // Sync F register after CB instruction
        return;
    }

    switch (opcode) {
        case 0x00: // NOP
            // No operation
            break;
        
        case 0x10: // STOP
            // STOP: Halt CPU and LCD until button press (or other condition)
            // In test context, just skip the next byte and continue
            mmu.read_byte(PC++);  // Read and skip the next byte (usually 0x00)
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
        case 0xED: // Undefined/illegal opcode, treat as NOP
            break;
        case 0xDD: // Undefined/illegal opcode, treat as NOP
            break;
        case 0x08: // LD (a16), SP
            {
                uint16_t addr = mmu.read_byte(PC) | (mmu.read_byte(PC + 1) << 8);
                PC += 2;
                mmu.write_byte(addr, SP & 0xFF);
                mmu.write_byte(addr + 1, SP >> 8);
            }
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
            {
                uint8_t low = mmu.read_byte(PC++);
                uint8_t high = mmu.read_byte(PC++);
                SP = low | (high << 8);
                // Debug: Print SP value
                //std::cout << "DEBUG: LD SP, nn = 0x" << std::hex << SP << std::dec << std::endl;
            }
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

        // ADD SP, n
        case 0xE8: // ADD SP, n
            {
                int8_t e = (int8_t)mmu.read_byte(PC++);
                uint16_t sp = SP;
                uint16_t result = sp + e;
                if (log_file.is_open()) {
                    log_file << "ADD SP,e: SP=0x" << std::hex << sp
                             << " e=" << std::dec << (int)e
                             << " result=0x" << std::hex << result << std::dec << std::endl;
                }
                // Flags: Z=0, N=0, H from bit3 carry, C from bit7 carry of low-byte add (SP low + e)
                zero_flag = false;
                subtract_flag = false;
                half_carry_flag = ((sp & 0x0F) + ((uint8_t)e & 0x0F)) > 0x0F;
                carry_flag      = ((sp & 0xFF) + (uint8_t)e) > 0xFF;
                SP = result;
                if (log_file.is_open()) {
                    log_file << "    Flags: Z=" << zero_flag << " N=" << subtract_flag
                             << " H=" << half_carry_flag << " C=" << carry_flag << std::endl;
                }
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
        // ADD n
        case 0xC6: // ADD A, n
            add(mmu.read_byte(PC++));
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
            {
                uint8_t imm = mmu.read_byte(PC++);
                uint8_t a_before = A;
                sub(imm);
                if (log_file.is_open()) {
                    log_file << "SUB A,n: A=0x" << std::hex << (int)a_before
                             << " n=0x" << (int)imm
                             << " -> A'=0x" << (int)A << std::dec
                             << " | Flags Z=" << zero_flag << " N=" << subtract_flag
                             << " H=" << half_carry_flag << " C=" << carry_flag << std::endl;
                }
            }
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
        // OR n
        case 0xF6: // OR n
            or_op(mmu.read_byte(PC++));
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
        case 0xEE: // XOR A, n
            xor_op(mmu.read_byte(PC++));
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
            {
                uint8_t imm = mmu.read_byte(PC++);
                uint8_t a_before = A;
                bool c_before = carry_flag;
                adc(imm);
                if (log_file.is_open()) {
                    log_file << "ADC A,n: A=0x" << std::hex << (int)a_before
                             << " n=0x" << (int)imm
                             << " C_in=" << (c_before?1:0)
                             << " -> A'=0x" << (int)A << std::dec
                             << " | Flags Z=" << zero_flag << " N=" << subtract_flag
                             << " H=" << half_carry_flag << " C=" << carry_flag << std::endl;
                }
            }
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
        case 0xDE: // SBC A, n
            sbc(mmu.read_byte(PC++));
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
            load_flags_from_f(); // Load flags from F register
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
            sync_f_register(); // Ensure F is up-to-date before push
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
            {
                int8_t off = (int8_t)mmu.read_byte(PC++);
                uint16_t pc_before = PC;
                bool take = !zero_flag;
                if (take) PC += off; else {/* not taken */}
                if (log_file.is_open()) {
                    log_file << "JR NZ, n: Z=" << zero_flag << " off=" << (int)off
                             << (take?" TAKEN":" SKIP")
                             << " PC: 0x" << std::hex << pc_before << " -> 0x" << PC << std::dec << std::endl;
                }
            }
            break;
        case 0x28: // JR Z, n
            {
                int8_t off = (int8_t)mmu.read_byte(PC++);
                uint16_t pc_before = PC;
                bool take = zero_flag;
                if (take) PC += off; else {/* not taken */}
                if (log_file.is_open()) {
                    log_file << "JR Z, n: Z=" << zero_flag << " off=" << (int)off
                             << (take?" TAKEN":" SKIP")
                             << " PC: 0x" << std::hex << pc_before << " -> 0x" << PC << std::dec << std::endl;
                }
            }
            break;
        case 0x30: // JR NC, n
            {
                int8_t off = (int8_t)mmu.read_byte(PC++);
                uint16_t pc_before = PC;
                bool take = !carry_flag;
                if (take) PC += off; else {/* not taken */}
                if (log_file.is_open()) {
                    log_file << "JR NC, n: C=" << carry_flag << " off=" << (int)off
                             << (take?" TAKEN":" SKIP")
                             << " PC: 0x" << std::hex << pc_before << " -> 0x" << PC << std::dec << std::endl;
                }
            }
            break;
        case 0x38: // JR C, n
            {
                int8_t off = (int8_t)mmu.read_byte(PC++);
                uint16_t pc_before = PC;
                bool take = carry_flag;
                if (take) PC += off; else {/* not taken */}
                if (log_file.is_open()) {
                    log_file << "JR C, n: C=" << carry_flag << " off=" << (int)off
                             << (take?" TAKEN":" SKIP")
                             << " PC: 0x" << std::hex << pc_before << " -> 0x" << PC << std::dec << std::endl;
                }
            }
            break;

        // Interrupt control
        case 0xF3: // DI (Disable Interrupts)
            ime = false;
            break;
        case 0xFB: // EI (Enable Interrupts) - takes effect after next instruction
            ei_delay_pending = true;
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

        // DAA (Decimal Adjust Accumulator)
        case 0x27: // DAA
            {
                uint8_t correction = 0;
                if (!subtract_flag) {
                    // After addition/increment
                    if (carry_flag || A > 0x99) {
                        correction |= 0x60;
                    }
                    if (half_carry_flag || (A & 0x0F) > 0x09) {
                        correction |= 0x06;
                    }
                    A = A + correction;
                    if (correction & 0x60) {
                        carry_flag = true;
                    }
                } else {
                    // After subtraction/decrement
                    if (carry_flag) {
                        correction |= 0x60;
                    }
                    if (half_carry_flag) {
                        correction |= 0x06;
                    }
                    A = A - correction;
                }
                zero_flag = (A == 0);
                half_carry_flag = false;
            }
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
            // zero_flag 不變
            break;

        // CCF (Complement Carry Flag)
        case 0x3F: // CCF
            carry_flag = !carry_flag;
            subtract_flag = false;
            half_carry_flag = false;
            // zero_flag 不變
            break;

        // HALT - with HALT bug implementation
        case 0x76: // HALT
            halted = true;
            // HALT bug: If IME is disabled but interrupts are pending,
            // CPU wakes up but PC is not incremented (stays at HALT instruction)
            if (!ime) {
                uint8_t ie_reg = mmu.read_byte(0xFFFF); // Interrupt Enable
                uint8_t if_reg = mmu.read_byte(0xFF0F); // Interrupt Flag
                if (ie_reg & if_reg) {
                    // HALT bug: PC stays at HALT instruction, don't increment PC
                    PC--; // Undo the PC++ from step() method
                }
            }
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
    
    // Sync F register from flags after instruction execution
    sync_f_register();
    
    // Log register state after instruction execution (for suspicious PC ranges)
    if (log_file.is_open() && (
        (PC >= 0xc000 && PC <= 0xc030) ||
        (PC >= 0x210 && PC <= 0x230)
    )) {
        log_file << "  State: A=0x" << std::hex << (int)A << " BC=0x" << BC
                 << " DE=0x" << DE << " HL=0x" << HL << " SP=0x" << SP
                 << " | Flags Z=" << std::dec << (zero_flag?1:0)
                 << " N=" << (subtract_flag?1:0) << " H=" << (half_carry_flag?1:0)
                 << " C=" << (carry_flag?1:0) << std::endl;
    }
}

int CPU::execute_instruction_with_cycles(uint8_t opcode) {

    int cycles = 4; // Default cycles

    // Handle CB prefix
    if (opcode == 0xCB) {
        uint8_t cb_opcode = mmu.read_byte(PC++);
        execute_cb_instruction(cb_opcode);
        return 8; // CB instructions take 8 cycles
    }

    // Set cycles based on instruction (timings per Pan Docs)
    // NOTE: Conditional instructions have two timings depending on whether the branch is taken.
    switch (opcode) {
        // 8 cycles (unconditional)
        case 0x03: case 0x13: case 0x23: case 0x33: // INC rr
        case 0x09: case 0x19: case 0x29: case 0x39: // ADD HL, rr
        case 0xF9: // LD SP, HL
            cycles = 8; break;

        // 12 cycles (unconditional)
        case 0x01: case 0x11: case 0x21: case 0x31: // LD rr, nn
        case 0x06: case 0x0E: case 0x16: case 0x1E: case 0x26: case 0x2E: case 0x36: case 0x3E: // LD r, n / LD (HL), n
        case 0xE0: case 0xF0: // LDH (n),A / LDH A,(n)
        case 0xF8: // LD HL, SP+n
        case 0xC1: case 0xD1: case 0xE1: case 0xF1: // POP rr
            cycles = 12; break;

        // 16 cycles (unconditional)
        case 0xEA: case 0xFA: // LD (nn),A / LD A,(nn)
        case 0xE8: // ADD SP, n
        case 0xC3: // JP nn
        case 0xC9: // RET
        case 0xD9: // RETI
        case 0xC7: case 0xCF: case 0xD7: case 0xDF: case 0xE7: case 0xEF: case 0xF7: case 0xFF: // RST t
            cycles = 16; break;

        // 20 cycles (unconditional)
        case 0x08: // LD (a16), SP
            cycles = 20; break;

        // 24 cycles (unconditional)
        case 0xCD: // CALL nn
            cycles = 24; break;

        // 16-bit PUSH (16 cycles)
        case 0xC5: case 0xD5: case 0xE5: case 0xF5: // PUSH rr
            cycles = 16; break;

        // Conditional JR (taken 12, not taken 8)
        case 0x20: cycles = (!zero_flag)  ? 12 : 8; break; // JR NZ,n
        case 0x28: cycles = ( zero_flag)  ? 12 : 8; break; // JR Z,n
        case 0x30: cycles = (!carry_flag) ? 12 : 8; break; // JR NC,n
        case 0x38: cycles = ( carry_flag) ? 12 : 8; break; // JR C,n

        // Conditional JP (taken 16, not taken 12)
        case 0xC2: cycles = (!zero_flag)  ? 16 : 12; break; // JP NZ,nn
        case 0xCA: cycles = ( zero_flag)  ? 16 : 12; break; // JP Z,nn
        case 0xD2: cycles = (!carry_flag) ? 16 : 12; break; // JP NC,nn
        case 0xDA: cycles = ( carry_flag) ? 16 : 12; break; // JP C,nn

        // Conditional RET (taken 20, not taken 8)
        case 0xC0: cycles = (!zero_flag)  ? 20 : 8; break; // RET NZ
        case 0xC8: cycles = ( zero_flag)  ? 20 : 8; break; // RET Z
        case 0xD0: cycles = (!carry_flag) ? 20 : 8; break; // RET NC
        case 0xD8: cycles = ( carry_flag) ? 20 : 8; break; // RET C

        // Conditional CALL (taken 24, not taken 12)
        case 0xC4: cycles = (!zero_flag)  ? 24 : 12; break; // CALL NZ,nn
        case 0xCC: cycles = ( zero_flag)  ? 24 : 12; break; // CALL Z,nn
        case 0xD4: cycles = (!carry_flag) ? 24 : 12; break; // CALL NC,nn
        case 0xDC: cycles = ( carry_flag) ? 24 : 12; break; // CALL C,nn

        default:
            // All other opcodes keep default 4 cycles here; many multi-cycle opcodes already handled above.
            break;
    }

    // Execute the instruction
    execute_instruction(opcode);
    return cycles;
}
