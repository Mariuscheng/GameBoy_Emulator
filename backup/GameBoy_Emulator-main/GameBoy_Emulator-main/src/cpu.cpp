#include "cpu.h"
#include <iostream>
#include <fstream>

CPU::CPU(MMU& mmu) : mmu(mmu) {
    reset();
    log_file.open("cpu_log.txt");
    // Instrumentation file (optional for instr_timing)
    instr_cycle_log.open("instr_cycles_log.txt", std::ios::out | std::ios::trunc);
    if (instr_cycle_log.is_open()) {
        instr_cycle_log << "# Instruction Cycle Log\n";
        instr_cycle_log.flush();
    } else {
        std::cout << "[WARN] instr_cycle_log failed to open" << std::endl;
    }
}

CPU::~CPU() {
    if (log_file.is_open()) {
        log_file.close();
    }
    if (instr_cycle_log.is_open()) {
        instr_cycle_log.close();
    }
    // std::cout << "[CPU SUMMARY] steps=" << step_count
    //           << " halt_count=" << halt_count
    //           << " halt_bug_count=" << halt_bug_count
    //           << std::endl;
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

    ime = false; // Interrupts DISABLED by default; ROM will EI when needed
    halted = false;
    just_woken_from_halt = false; // Not woken from halt on reset
    ei_delay_pending = false; // No EI delay pending on reset
    halt_bug_active = false; // No HALT bug pending
    step_count = 0;
    halt_count = 0;
    halt_bug_count = 0;
}

int CPU::step() {
    step_count++;
    //if (step_count % 10000 == 0) {
    //    std::cout << "[CPU] Executed " << step_count << " steps, current PC=" << PC << std::endl;
    //}

    // If CPU is halted, only wake when an ENABLED interrupt is pending (IE & IF)
    if (halted) {
        uint8_t ie_reg = mmu.read_byte(0xFFFF); // Interrupt Enable
        uint8_t if_reg = mmu.read_byte(0xFF0F); // Interrupt Flag
        uint8_t enabled_pending = (ie_reg & if_reg) & 0x1F; // Mask to lower 5 bits

        if (enabled_pending) {
            halted = false;
            just_woken_from_halt = true; // Mark wake for potential immediate interrupt service
        } else {
            // Remain halted: consume 4 cycles (1 M-cycle)
            mmu.update_timer_cycles(4);
            return 4;
        }
    }

    uint8_t opcode;
    if (halt_bug_active) {
        // HALT bug: re-fetch same opcode (PC already points to next instruction due to normal increment during HALT execution)
        opcode = mmu.read_byte(PC); // Do NOT increment PC this fetch
        halt_bug_active = false; // One-shot effect
        // Optional debug
        // std::cout << "[CPU] HALT bug fetch at PC=" << std::hex << PC << std::dec << " opcode=0x" << std::hex << (int)opcode << std::dec << std::endl;
    } else {
        opcode = mmu.read_byte(PC++);
    }
    // Expanded logging: first 200 instructions OR targeted PC window
    if (step_count < 200 || (PC >= 0x50 && PC <= 0x60)) {
        std::cout << "[CPU] step=" << step_count << " PC=" << std::hex << (PC-1)
                  << std::dec << " opcode=0x" << std::hex << (int)opcode << std::dec
                  << " IME=" << (int)ime << " IF=" << std::hex << (int)mmu.read_byte(0xFF0F)
                  << " IE=" << (int)mmu.read_byte(0xFFFF) << std::dec << std::endl;
    }
    // In timing test mode, model M1 (opcode fetch) as 4 T-cycles before executing the body
    if (timing_test_mode) {
        burn_tcycles(4); // M1 opcode fetch
    }
    int cycles = execute_instruction_with_cycles(opcode);
    if (timing_test_mode) {
        // We already burned 4 T-cycles for M1 above; exclude them from the count
        if (cycles >= 4) cycles -= 4; else cycles = 0;
    }

    // Update timer based on instruction cycles
    mmu.update_timer_cycles(static_cast<uint8_t>(cycles));

    // Apply EI delay - EI takes effect after the next instruction executes
    if (ei_delay_pending) {
        ime = true;
        ei_delay_pending = false;
    }

    // Check for interrupts after instruction execution
    // HALT bug simplified: we currently do NOT service interrupts when IME=0.
    // A full implementation needs special PC increment glitch; for test 02 (interrupts) we restrict to IME only.
    if (ime) {
        uint8_t ie_reg = mmu.read_byte(0xFFFF); // Interrupt Enable
        uint8_t if_reg = mmu.read_byte(0xFF0F); // Interrupt Flag

        uint8_t interrupts = ie_reg & if_reg;
        if (interrupts) {
            //std::cout << "[CPU] Interrupt pending at PC=" << std::hex << PC << std::dec << "! IE=" << (int)ie_reg << " IF=" << (int)if_reg << " combined=" << (int)interrupts << std::endl;
            halted = false; // Wake up from HALT
            just_woken_from_halt = false; // Clear the flag
            // Find highest priority interrupt
            for (int i = 0; i < 5; ++i) {
                if (interrupts & (1 << i)) {
                    //std::cout << "[CPU] Handling interrupt " << i << " at PC=" << std::hex << PC << std::dec << std::endl;
                    // Disable interrupts and jump to handler
                    ime = false;
                    uint8_t if_clear = mmu.read_byte(0xFF0F) & ~(1 << i);
                    mmu.write_byte(0xFF0F, if_clear);
                    
                    // Push PC to stack
                    SP -= 2;
                    mmu.write_byte(SP, PC & 0xFF);
                    mmu.write_byte(SP + 1, PC >> 8);
                    
                    // Jump to interrupt vector
                    static const uint16_t vectors[5] = {0x40, 0x48, 0x50, 0x58, 0x60};
                    PC = vectors[i];
                    
                    // Interrupt handling takes 5 cycles (2 for delay + 3 for jump)
                    // Interrupt service routine timing:
                    // We already accounted for instruction cycles above; now add 5 M-cycles (20 T-cycles)
                    mmu.update_timer_cycles(5);
                    return cycles + 5; // Report total cycles including interrupt handling
                }
            }
        }
        just_woken_from_halt = false; // Clear any wake flag (not used for interrupt servicing now)
    }

    //std::cout << "[CPU] Step end PC=" << PC << " cycles=" << cycles << std::endl;
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
        {
            // STOP: Halt CPU and LCD until button press (or other condition)
            // In test context, we need to properly handle STOP
            uint8_t stop_param = mmu.read_byte(PC++);  // Read the stop parameter
            std::cout << "[CPU] Executing STOP with param 0x" << std::hex << (int)stop_param << std::dec << std::endl;
            
            // In headless test mode, STOP should halt execution until an interrupt occurs
            // For now, we'll continue but set a flag to indicate we're in STOP mode
            // The CPU should wake up on interrupts even when IME=0
            halted = true;
            std::cout << "[CPU] CPU halted by STOP instruction" << std::endl;
            break;
        }

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
    // (Removed misplaced cycle diagnostic here; cycle logging handled in execute_instruction_with_cycles)
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
            if (timing_test_mode) {
                B = mmu.read_byte(HL);       // M2: read at start (M1 already burned in step())
                burn_tcycles(4);             // finish M2
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
                sub(imm);
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
                if (timing_test_mode) {
                    uint8_t value = mmu.read_byte(HL);   // M2 read at start (T4)
                    burn_tcycles(4);                      // M2 complete
                    inc(value);                           // Modify during M2->M3 transition
                    burn_tcycles(2);                      // M3 setup
                    mmu.write_byte(HL, value);            // M3 write at T10-11 (T2-3 of M3)
                    burn_tcycles(2);                      // M3 complete
                } else {
                    uint8_t value = mmu.read_byte(HL);
                    inc(value);
                    mmu.write_byte(HL, value);
                }
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
                if (timing_test_mode) {
                    uint8_t value = mmu.read_byte(HL);   // M2 read at start (T4)
                    burn_tcycles(4);                      // M2 complete
                    dec(value);                           // Modify during M2->M3 transition
                    burn_tcycles(2);                      // M3 setup
                    mmu.write_byte(HL, value);            // M3 write at T10-11 (T2-3 of M3)
                    burn_tcycles(2);                      // M3 complete
                } else {
                    uint8_t value = mmu.read_byte(HL);
                    dec(value);
                    mmu.write_byte(HL, value);
                }
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
                bool take = !zero_flag;
                if (take) PC += off;
            }
            break;
        case 0x28: // JR Z, n
            {
                int8_t off = (int8_t)mmu.read_byte(PC++);
                bool take = zero_flag;
                if (take) PC += off;
            }
            break;
        case 0x30: // JR NC, n
            {
                int8_t off = (int8_t)mmu.read_byte(PC++);
                bool take = !carry_flag;
                if (take) PC += off;
            }
            break;
        case 0x38: // JR C, n
            {
                int8_t off = (int8_t)mmu.read_byte(PC++);
                bool take = carry_flag;
                if (take) PC += off;
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
        {
            uint8_t ie_reg = mmu.read_byte(0xFFFF);
            uint8_t if_reg = mmu.read_byte(0xFF0F);
            uint8_t pending_enabled = (ie_reg & if_reg) & 0x1F; // Only interrupts that are both requested and enabled
            // 正確 HALT bug 條件 (Pan Docs): IME=0 且存在已啟用且已請求的中斷 (IE & IF != 0)
            // 在此情況下：CPU 不會進入真正的 HALT；下一次 opcode 取值會重複讀取 HALT 之後的那一個位元組（造成後續指令位元組被重複執行一次）
            if (!ime && pending_enabled) {
                halt_bug_active = true;   // 一次性：下一次取指不遞增 PC
                halted = false;          // 不進入 halted 狀態
                // 可選除錯輸出：
                // std::cout << "[CPU] HALT bug (enabled) PC=" << std::hex << (PC-1) << std::dec << " IE=" << (int)ie_reg << " IF=" << (int)if_reg << std::endl;
            } else {
                // 正常 HALT：直到有『已啟用且已請求』的中斷出現才醒 (即 IE & IF !=0)
                halted = true;
                // std::cout << "[CPU] HALT normal PC=" << std::hex << (PC-1) << std::dec << " IME=" << (int)ime << std::endl;
            }
            break;
        }

        // RST instructions (Restart)
        case 0xC7: // RST 00H
            // RST: Push current PC then jump to vector
            SP -= 2; mmu.write_byte(SP, PC & 0xFF); mmu.write_byte(SP + 1, PC >> 8); PC = 0x00;
            break;
        case 0xCF: // RST 08H
            SP -= 2; mmu.write_byte(SP, PC & 0xFF); mmu.write_byte(SP + 1, PC >> 8); PC = 0x08;
            break;
        case 0xD7: // RST 10H
            SP -= 2; mmu.write_byte(SP, PC & 0xFF); mmu.write_byte(SP + 1, PC >> 8); PC = 0x10;
            break;
        case 0xDF: // RST 18H
            SP -= 2; mmu.write_byte(SP, PC & 0xFF); mmu.write_byte(SP + 1, PC >> 8); PC = 0x18;
            break;
        case 0xE7: // RST 20H
            SP -= 2; mmu.write_byte(SP, PC & 0xFF); mmu.write_byte(SP + 1, PC >> 8); PC = 0x20;
            break;
        case 0xEF: // RST 28H
            SP -= 2; mmu.write_byte(SP, PC & 0xFF); mmu.write_byte(SP + 1, PC >> 8); PC = 0x28;
            break;
        case 0xF7: // RST 30H
            SP -= 2; mmu.write_byte(SP, PC & 0xFF); mmu.write_byte(SP + 1, PC >> 8); PC = 0x30;
            break;
        case 0xFF: // RST 38H
            SP -= 2; mmu.write_byte(SP, PC & 0xFF); mmu.write_byte(SP + 1, PC >> 8); PC = 0x38;
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
}

int CPU::execute_instruction_with_cycles(uint8_t opcode) {
    // Reset per-instruction burned T-cycles accounting
    timing_burned_tcycles = 0;
    // Expected cycle table (machine cycles) for non-CB opcodes (Pan Docs). CB handled separately.
    // NOTE: Conditional instructions use max cycles when taken; we will compute actual below.
    static const int expected_cycles[256] = {
        /*00*/4,/*01*/12,/*02*/8,/*03*/8,/*04*/4,/*05*/4,/*06*/8,/*07*/4, /*08*/20,/*09*/8,/*0A*/8,/*0B*/8,/*0C*/4,/*0D*/4,/*0E*/8,/*0F*/4,
        /*10*/4,/*11*/12,/*12*/8,/*13*/8,/*14*/4,/*15*/4,/*16*/8,/*17*/4, /*18*/12,/*19*/8,/*1A*/8,/*1B*/8,/*1C*/4,/*1D*/4,/*1E*/8,/*1F*/4,
        /*20*/12,/*21*/12,/*22*/8,/*23*/8,/*24*/4,/*25*/4,/*26*/8,/*27*/4, /*28*/12,/*29*/8,/*2A*/8,/*2B*/8,/*2C*/4,/*2D*/4,/*2E*/8,/*2F*/4,
        /*30*/12,/*31*/12,/*32*/8,/*33*/8,/*34*/12,/*35*/12,/*36*/12,/*37*/4, /*38*/12,/*39*/8,/*3A*/8,/*3B*/8,/*3C*/4,/*3D*/4,/*3E*/8,/*3F*/4,
        /*40*/4,/*41*/4,/*42*/4,/*43*/4,/*44*/4,/*45*/4,/*46*/8,/*47*/4, /*48*/4,/*49*/4,/*4A*/4,/*4B*/4,/*4C*/4,/*4D*/4,/*4E*/8,/*4F*/4,
        /*50*/4,/*51*/4,/*52*/4,/*53*/4,/*54*/4,/*55*/4,/*56*/8,/*57*/4, /*58*/4,/*59*/4,/*5A*/4,/*5B*/4,/*5C*/4,/*5D*/4,/*5E*/8,/*5F*/4,
        /*60*/4,/*61*/4,/*62*/4,/*63*/4,/*64*/4,/*65*/4,/*66*/8,/*67*/4, /*68*/4,/*69*/4,/*6A*/4,/*6B*/4,/*6C*/4,/*6D*/4,/*6E*/8,/*6F*/4,
        /*70*/8,/*71*/8,/*72*/8,/*73*/8,/*74*/8,/*75*/8,/*76*/4,/*77*/8, /*78*/4,/*79*/4,/*7A*/4,/*7B*/4,/*7C*/4,/*7D*/4,/*7E*/8,/*7F*/4,
        /*80*/4,/*81*/4,/*82*/4,/*83*/4,/*84*/4,/*85*/4,/*86*/8,/*87*/4, /*88*/4,/*89*/4,/*8A*/4,/*8B*/4,/*8C*/4,/*8D*/4,/*8E*/8,/*8F*/4,
        /*90*/4,/*91*/4,/*92*/4,/*93*/4,/*94*/4,/*95*/4,/*96*/8,/*97*/4, /*98*/4,/*99*/4,/*9A*/4,/*9B*/4,/*9C*/4,/*9D*/4,/*9E*/8,/*9F*/4,
        /*A0*/4,/*A1*/4,/*A2*/4,/*A3*/4,/*A4*/4,/*A5*/4,/*A6*/8,/*A7*/4, /*A8*/4,/*A9*/4,/*AA*/4,/*AB*/4,/*AC*/4,/*AD*/4,/*AE*/8,/*AF*/4,
        /*B0*/4,/*B1*/4,/*B2*/4,/*B3*/4,/*B4*/4,/*B5*/4,/*B6*/8,/*B7*/4, /*B8*/4,/*B9*/4,/*BA*/4,/*BB*/4,/*BC*/4,/*BD*/4,/*BE*/8,/*BF*/4,
        /*C0*/8,/*C1*/12,/*C2*/12,/*C3*/16,/*C4*/12,/*C5*/16,/*C6*/8,/*C7*/16, /*C8*/8,/*C9*/16,/*CA*/12,/*CB*/8,/*CC*/12,/*CD*/24,/*CE*/8,/*CF*/16,
        /*D0*/8,/*D1*/12,/*D2*/12,/*D3*/4,/*D4*/12,/*D5*/16,/*D6*/8,/*D7*/16, /*D8*/8,/*D9*/16,/*DA*/12,/*DB*/4,/*DC*/12,/*DD*/4,/*DE*/8,/*DF*/16,
        /*E0*/12,/*E1*/12,/*E2*/8,/*E3*/4,/*E4*/4,/*E5*/16,/*E6*/8,/*E7*/16, /*E8*/16,/*E9*/4,/*EA*/16,/*EB*/4,/*EC*/4,/*ED*/4,/*EE*/8,/*EF*/16,
        /*F0*/12,/*F1*/12,/*F2*/8,/*F3*/4,/*F4*/4,/*F5*/16,/*F6*/8,/*F7*/16, /*F8*/12,/*F9*/8,/*FA*/16,/*FB*/4,/*FC*/4,/*FD*/4,/*FE*/8,/*FF*/16
    };

    // Reference: Pan Docs / GB CPU timings. Values represent T-cycles (4T per M-cycle).
    int cycles = 4; // Default for simple register ops

    // Note: Do NOT align instruction start to any external phase.
    // Real hardware proceeds continuously; artificial alignment causes jitter in tests.

    // Handle CB prefix first (special timing: 8 or 16 depending on operand)
    if (opcode == 0xCB) {
        // Micro-step: represent M2 timing for CB sub-opcode fetch (M1 burned in step())
        uint8_t cb_opcode = mmu.read_byte(PC++); // fetch CB sub-opcode
        if (timing_test_mode) {
            burn_tcycles(4); // M2 (cb sub-opcode fetch)
        }
        bool operand_is_hl = (cb_opcode & 0x07) == 0x06;
        uint8_t operation = cb_opcode >> 3; // 0-31 groups (RLC..SET)
        bool is_bit = (operation >= 8 && operation <= 15);
        
        // For (HL) operand with timing_test_mode, handle read-modify-write timing
        if (timing_test_mode && operand_is_hl) {
            // CB (HL) instructions: 16 cycles = M1(4) + M2(4) + M3(4) + M4(4)
            // BIT (HL): 12 cycles = M1(4) + M2(4) + M3(4) [no write]
            // Read happens at start of M3, write at start of M4
            
            uint8_t value = mmu.read_byte(HL);    // M3 read at start (T8)
            burn_tcycles(4);                       // M3 complete
            
            // Now execute the operation (modify phase)
            if (is_bit) {
                // BIT operations - only test, no write
                uint8_t bit = operation - 8;
                zero_flag = (value & (1 << bit)) == 0;
                subtract_flag = false;
                half_carry_flag = true;
                // No M4 for BIT, done at 12 cycles
            } else {
                // RLC/RRC/RL/RR/SLA/SRA/SWAP/SRL/RES/SET - all write
                uint8_t result;
                if (operation == 0) { // RLC
                    carry_flag = (value & 0x80) != 0;
                    result = (value << 1) | (carry_flag ? 1 : 0);
                    zero_flag = result == 0;
                    subtract_flag = false;
                    half_carry_flag = false;
                } else if (operation == 1) { // RRC
                    carry_flag = (value & 0x01) != 0;
                    result = (value >> 1) | (carry_flag ? 0x80 : 0);
                    zero_flag = result == 0;
                    subtract_flag = false;
                    half_carry_flag = false;
                } else if (operation == 2) { // RL
                    bool old_carry = carry_flag;
                    carry_flag = (value & 0x80) != 0;
                    result = (value << 1) | (old_carry ? 1 : 0);
                    zero_flag = result == 0;
                    subtract_flag = false;
                    half_carry_flag = false;
                } else if (operation == 3) { // RR
                    bool old_carry = carry_flag;
                    carry_flag = (value & 0x01) != 0;
                    result = (value >> 1) | (old_carry ? 0x80 : 0);
                    zero_flag = result == 0;
                    subtract_flag = false;
                    half_carry_flag = false;
                } else if (operation == 4) { // SLA
                    carry_flag = (value & 0x80) != 0;
                    result = value << 1;
                    zero_flag = result == 0;
                    subtract_flag = false;
                    half_carry_flag = false;
                } else if (operation == 5) { // SRA
                    carry_flag = (value & 0x01) != 0;
                    result = (value >> 1) | (value & 0x80);
                    zero_flag = result == 0;
                    subtract_flag = false;
                    half_carry_flag = false;
                } else if (operation == 6) { // SWAP
                    result = ((value & 0x0F) << 4) | ((value & 0xF0) >> 4);
                    zero_flag = result == 0;
                    subtract_flag = false;
                    half_carry_flag = false;
                    carry_flag = false;
                } else if (operation == 7) { // SRL
                    carry_flag = (value & 0x01) != 0;
                    result = value >> 1;
                    zero_flag = result == 0;
                    subtract_flag = false;
                    half_carry_flag = false;
                } else if (operation >= 16 && operation <= 23) { // RES
                    uint8_t bit = operation - 16;
                    result = value & ~(1 << bit);
                } else if (operation >= 24 && operation <= 31) { // SET
                    uint8_t bit = operation - 24;
                    result = value | (1 << bit);
                } else {
                    result = value; // Fallback
                }
                
                // Write phase: M4
                burn_tcycles(2);                       // M4 setup
                mmu.write_byte(HL, result);            // M4 write at T14-15 (T2-3 of M4)
                burn_tcycles(2);                       // M4 complete
            }
            sync_f_register(); // Sync F register after CB instruction
        } else {
            // Non-(HL) operands or non-timing-test-mode
            execute_cb_instruction(cb_opcode);
        }
        
        int real_cb_cycles;
        if (operand_is_hl) {
            // BIT (HL) is 12 cycles; all other (HL) CB ops are 16.
            real_cb_cycles = is_bit ? 12 : 16;
        } else {
            real_cb_cycles = 8; // All register CB ops are 8 cycles.
        }
        if (timing_test_mode && timing_burned_tcycles > 0) {
            int adjusted = real_cb_cycles - timing_burned_tcycles;
            if (adjusted < 0) adjusted = 0;
            real_cb_cycles = adjusted;
        }
        int expected_cb_cycles = real_cb_cycles; // Using Pan Docs rules above
        if (instr_cycle_log.is_open()) {
            instr_cycle_log << "CB 0x" << std::hex << (int)cb_opcode << std::dec
                            << " cycles=" << real_cb_cycles << " expected=" << expected_cb_cycles
                            << (real_cb_cycles==expected_cb_cycles?" OK":" MISMATCH") << '\n';
        }
        return real_cb_cycles;
    }

    // New diagnostic entry point for CP immediate opcode
    if (opcode == 0xFE && instr_cycle_log.is_open()) {
        instr_cycle_log << "[DEBUG] enter execute_instruction_with_cycles 0xFE initial cycles=" << cycles << "\n";
    }

    switch (opcode) {
        // 8-bit immediate loads (register only) 8 cycles
        case 0x06: case 0x0E: case 0x16: case 0x1E: case 0x26: case 0x2E: case 0x3E:
            cycles = 8; break; // LD r,n
        case 0x36: // LD (HL),n
            cycles = 12; break;

        // 16-bit loads
        case 0x01: case 0x11: case 0x21: case 0x31: // LD rr,nn
            cycles = 12; break;

        // LD (a16),SP
        case 0x08: cycles = 20; break;

        // LD A,(rr) & LD (rr),A
        case 0x0A: case 0x1A: case 0x02: case 0x12: cycles = 8; break;

        // LDH (n),A / LDH A,(n)
        case 0xE0: case 0xF0: cycles = 12; break;
        // LD (C),A / LD A,(C)
        case 0xE2: case 0xF2: cycles = 8; break;

        // LD (nn),A / LD A,(nn)
        case 0xEA: case 0xFA: cycles = 16; break;

        // LD HL,SP+e
        case 0xF8: cycles = 12; break;
        // LD SP,HL
        case 0xF9: cycles = 8; break;

        // ADD SP,e
        case 0xE8: cycles = 16; break;

        // INC/DEC 16-bit
        case 0x03: case 0x13: case 0x23: case 0x33: // INC rr
        case 0x0B: case 0x1B: case 0x2B: case 0x3B: // DEC rr
            cycles = 8; break;

        // ADD HL,rr
        case 0x09: case 0x19: case 0x29: case 0x39: cycles = 8; break;

        // PUSH rr
        case 0xC5: case 0xD5: case 0xE5: case 0xF5: cycles = 16; break;
        // POP rr
        case 0xC1: case 0xD1: case 0xE1: case 0xF1: cycles = 12; break;

        // ALU with (HL) memory operand (8 cycles)
        case 0x86: case 0x8E: case 0x96: case 0x9E: // ADD/ADC/SUB/SBC A,(HL)
        case 0xA6: case 0xAE: case 0xB6: case 0xBE: // AND/XOR/OR/CP (HL)
            cycles = 8; break;

        // INC/DEC (HL)
        case 0x34: case 0x35: cycles = 12; break;

        // LD r,(HL) and LD (HL),r (all 8 cycles)
        case 0x46: case 0x4E: case 0x56: case 0x5E: case 0x66: case 0x6E: case 0x7E: // LD r,(HL)
        case 0x70: case 0x71: case 0x72: case 0x73: case 0x74: case 0x75: case 0x77: // LD (HL),r
            cycles = 8; break;

        // LDI / LDD block transfer instructions
        case 0x22: // LDI (HL),A
        case 0x2A: // LDI A, (HL)
        case 0x32: // LDD (HL),A
        case 0x3A: // LDD A, (HL)
            cycles = 8; break;

        // ALU immediate operations (8 cycles)
        // ADD A,d8 (0xC6), ADC A,d8 (0xCE), SUB A,d8 (0xD6), SBC A,d8 (0xDE),
        // AND d8 (0xE6), XOR d8 (0xEE), OR d8 (0xF6), CP d8 (0xFE)
        case 0xC6: case 0xCE: case 0xD6: case 0xDE: case 0xE6: case 0xEE: case 0xF6: case 0xFE:
            cycles = 8; 
            if (opcode == 0xFE && instr_cycle_log.is_open()) {
                instr_cycle_log << "[DEBUG] cycle assign CP d8 -> 8\n";
            }
            break;

        // JR r8
        case 0x18: cycles = 12; break;
        // Conditional JR (taken 12 / not taken 8)
        case 0x20: cycles = (!zero_flag)  ? 12 : 8; break;
        case 0x28: cycles = ( zero_flag)  ? 12 : 8; break;
        case 0x30: cycles = (!carry_flag) ? 12 : 8; break;
        case 0x38: cycles = ( carry_flag) ? 12 : 8; break;

        // JP nn
        case 0xC3: cycles = 16; break;
        // JP (HL)
        case 0xE9: cycles = 4; break;
        // Conditional JP (taken 16 / not 12)
        case 0xC2: cycles = (!zero_flag)  ? 16 : 12; break;
        case 0xCA: cycles = ( zero_flag)  ? 16 : 12; break;
        case 0xD2: cycles = (!carry_flag) ? 16 : 12; break;
        case 0xDA: cycles = ( carry_flag) ? 16 : 12; break;

        // CALL nn
        case 0xCD: cycles = 24; break;
        // Conditional CALL (taken 24 / not 12)
        case 0xC4: cycles = (!zero_flag)  ? 24 : 12; break;
        case 0xCC: cycles = ( zero_flag)  ? 24 : 12; break;
        case 0xD4: cycles = (!carry_flag) ? 24 : 12; break;
        case 0xDC: cycles = ( carry_flag) ? 24 : 12; break;

        // RET / RETI
        case 0xC9: case 0xD9: cycles = 16; break;
        // Conditional RET (taken 20 / not 8)
        case 0xC0: cycles = (!zero_flag)  ? 20 : 8; break;
        case 0xC8: cycles = ( zero_flag)  ? 20 : 8; break;
        case 0xD0: cycles = (!carry_flag) ? 20 : 8; break;
        case 0xD8: cycles = ( carry_flag) ? 20 : 8; break;

        // RST
        case 0xC7: case 0xCF: case 0xD7: case 0xDF: case 0xE7: case 0xEF: case 0xF7: case 0xFF: cycles = 16; break;

        // Misc with longer timings already handled above; others fall through to default 4 cycles.
        default: break;
    }

    // Diagnostic: confirm cycle assignment for CP d8 (0xFE)
    if (opcode == 0xFE && instr_cycle_log.is_open()) {
        instr_cycle_log << "[DEBUG] post-switch cycles for 0xFE=" << cycles << "\n";
    }

    // Execute the instruction core logic
    execute_instruction(opcode);
    int reported = cycles;
    if (timing_test_mode && timing_burned_tcycles > 0) {
        // We already advanced PPU/APU/TIMA by timing_burned_tcycles inside the instruction;
        // subtract them from the cycles we report so outer loop doesn't double-step.
        if (reported >= timing_burned_tcycles) reported -= timing_burned_tcycles;
    }
    // Conditional adjustments: overwrite 'reported' but keep 'cycles' proper
    // We already computed actual above.

    // Logging & mismatch detection
    if (instr_cycle_log.is_open()) {
        int expected = expected_cycles[opcode];
        // For conditional instructions we stored non-taken value in expected table (Pan Docs lists non-taken); when taken we add difference.
        // We can recompute expected dynamic for branch opcodes to reduce false positives.
        switch (opcode) {
            case 0x20: expected = (!zero_flag)?12:8; break;
            case 0x28: expected = ( zero_flag)?12:8; break;
            case 0x30: expected = (!carry_flag)?12:8; break;
            case 0x38: expected = ( carry_flag)?12:8; break;
            case 0xC2: expected = (!zero_flag)?16:12; break;
            case 0xCA: expected = ( zero_flag)?16:12; break;
            case 0xD2: expected = (!carry_flag)?16:12; break;
            case 0xDA: expected = ( carry_flag)?16:12; break;
            case 0xC0: expected = (!zero_flag)?20:8; break;
            case 0xC8: expected = ( zero_flag)?20:8; break;
            case 0xD0: expected = (!carry_flag)?20:8; break;
            case 0xD8: expected = ( carry_flag)?20:8; break;
            case 0xC4: expected = (!zero_flag)?24:12; break;
            case 0xCC: expected = ( zero_flag)?24:12; break;
            case 0xD4: expected = (!carry_flag)?24:12; break;
            case 0xDC: expected = ( carry_flag)?24:12; break;
        }
        instr_cycle_log << "OP 0x" << std::hex << (int)opcode << std::dec
                        << " cycles=" << reported << " expected=" << expected
                        << (reported==expected?" OK":" MISMATCH") << '\n';
    }
    return reported;
}
