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
    // Inform MMU of the current PC to aid in VRAM write logging (debug only)
    // This allows MMU to log the PC of the CPU when writes to VRAM happen.
    mmu.set_last_cpu_pc(PC);
    if (halt_bug_active) {
        // HALT bug: re-fetch same opcode (PC already points to next instruction due to normal increment during HALT execution)
        opcode = mmu.read_byte(PC); // Do NOT increment PC this fetch
        halt_bug_active = false; // One-shot effect
        // Optional debug
        // std::cout << "[CPU] HALT bug fetch at PC=" << std::hex << PC << std::dec << " opcode=0x" << std::hex << (int)opcode << std::dec << std::endl;
    } else {
        opcode = mmu.read_byte(PC++);
    }
    // Expanded logging: first 200 instructions OR targeted PC window (optional via GB_CPU_DEBUG)
#if GB_CPU_DEBUG
    if (step_count < 200 || (PC >= 0x50 && PC <= 0x60)) {
        // Avoid printing the first step if it is undesired (suppress known 'bedbug' noise)
        if (step_count != 1) {
            std::cout << "[CPU] step=" << step_count << " PC=" << std::hex << (PC-1)
                      << std::dec << " opcode=0x" << std::hex << (int)opcode << std::dec
                      << " IME=" << (int)ime << " IF=" << std::hex << (int)mmu.read_byte(0xFF0F)
                      << " IE=" << (int)mmu.read_byte(0xFFFF) << std::dec << std::endl;
        }
    }
#endif
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

void CPU::execute_stop_instruction() {
    // STOP: Halt CPU and LCD until button press (or other condition)
    // In test context, we need to properly handle STOP
    uint8_t stop_param = mmu.read_byte(PC++);  // Read the stop parameter

    // In headless test mode, STOP should halt execution until an interrupt occurs
    // For now, we'll continue but set a flag to indicate we're in STOP mode
    // The CPU should wake up on interrupts even when IME=0
    halted = true;
}

void CPU::execute_arithmetic_instructions(uint8_t opcode) {
    switch (opcode) {
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

        // ADD A, r
        case 0x80: add(B); break;
        case 0x81: add(C); break;
        case 0x82: add(D); break;
        case 0x83: add(E); break;
        case 0x84: add(H); break;
        case 0x85: add(L); break;
        case 0x86: add(mmu.read_byte(HL)); break;
        case 0x87: add(A); break;
        case 0xC6: add(mmu.read_byte(PC++)); break;

        // ADC A, r
        case 0x88: adc(B); break;
        case 0x89: adc(C); break;
        case 0x8A: adc(D); break;
        case 0x8B: adc(E); break;
        case 0x8C: adc(H); break;
        case 0x8D: adc(L); break;
        case 0x8E: adc(mmu.read_byte(HL)); break;
        case 0x8F: adc(A); break;
        case 0xCE: adc(mmu.read_byte(PC++)); break;

        // SUB A, r
        case 0x90: sub(B); break;
        case 0x91: sub(C); break;
        case 0x92: sub(D); break;
        case 0x93: sub(E); break;
        case 0x94: sub(H); break;
        case 0x95: sub(L); break;
        case 0x96: sub(mmu.read_byte(HL)); break;
        case 0x97: sub(A); break;
        case 0xD6: sub(mmu.read_byte(PC++)); break;

        // SBC A, r
        case 0x98: sbc(B); break;
        case 0x99: sbc(C); break;
        case 0x9A: sbc(D); break;
        case 0x9B: sbc(E); break;
        case 0x9C: sbc(H); break;
        case 0x9D: sbc(L); break;
        case 0x9E: sbc(mmu.read_byte(HL)); break;
        case 0x9F: sbc(A); break;
        case 0xDE: sbc(mmu.read_byte(PC++)); break;

        // CP A, r
        case 0xB8: cp(B); break;
        case 0xB9: cp(C); break;
        case 0xBA: cp(D); break;
        case 0xBB: cp(E); break;
        case 0xBC: cp(H); break;
        case 0xBD: cp(L); break;
        case 0xBE: cp(mmu.read_byte(HL)); break;
        case 0xBF: cp(A); break;
        case 0xFE: cp(mmu.read_byte(PC++)); break;

        // ADD SP, n
        case 0xE8: {
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
            break;
        }
    }
}

void CPU::execute_logical_instructions(uint8_t opcode) {
    switch (opcode) {
        // AND A, r
        case 0xA0: and_op(B); break;
        case 0xA1: and_op(C); break;
        case 0xA2: and_op(D); break;
        case 0xA3: and_op(E); break;
        case 0xA4: and_op(H); break;
        case 0xA5: and_op(L); break;
        case 0xA6: and_op(mmu.read_byte(HL)); break;
        case 0xA7: and_op(A); break;
        case 0xE6: and_op(mmu.read_byte(PC++)); break;

        // OR A, r
        case 0xB0: or_op(B); break;
        case 0xB1: or_op(C); break;
        case 0xB2: or_op(D); break;
        case 0xB3: or_op(E); break;
        case 0xB4: or_op(H); break;
        case 0xB5: or_op(L); break;
        case 0xB6: or_op(mmu.read_byte(HL)); break;
        case 0xB7: or_op(A); break;
        case 0xF6: or_op(mmu.read_byte(PC++)); break;

        // XOR A, r
        case 0xA8: xor_op(B); break;
        case 0xA9: xor_op(C); break;
        case 0xAA: xor_op(D); break;
        case 0xAB: xor_op(E); break;
        case 0xAC: xor_op(H); break;
        case 0xAD: xor_op(L); break;
        case 0xAE: xor_op(mmu.read_byte(HL)); break;
        case 0xAF: xor_op(A); break;
        case 0xEE: xor_op(mmu.read_byte(PC++)); break;
    }
}

void CPU::execute_inc_dec_instructions(uint8_t opcode) {
    switch (opcode) {
        // INC r
        case 0x04: inc(B); break;
        case 0x0C: inc(C); break;
        case 0x14: inc(D); break;
        case 0x1C: inc(E); break;
        case 0x24: inc(H); break;
        case 0x2C: inc(L); break;
        case 0x34: {
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
            break;
        }
        case 0x3C: inc(A); break;

        // DEC r
        case 0x05: dec(B); break;
        case 0x0D: dec(C); break;
        case 0x15: dec(D); break;
        case 0x1D: dec(E); break;
        case 0x25: dec(H); break;
        case 0x2D: dec(L); break;
        case 0x35: {
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
            break;
        }
        case 0x3D: dec(A); break;

        // INC rr
        case 0x03: BC++; break;
        case 0x13: DE++; break;
        case 0x23: HL++; break;
        case 0x33: SP++; break;

        // DEC rr
        case 0x0B: BC--; break;
        case 0x1B: DE--; break;
        case 0x2B: HL--; break;
        case 0x3B: SP--; break;
    }
}

void CPU::execute_stack_instructions(uint8_t opcode) {
    switch (opcode) {
        // POP rr
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

        // PUSH rr
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
    }
}

void CPU::execute_jump_instructions(uint8_t opcode) {
    switch (opcode) {
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

        // JR instructions
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
    }
}

void CPU::execute_call_return_instructions(uint8_t opcode) {
    switch (opcode) {
        // CALL instructions
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

        // RET instructions
        case 0xC9: // RET
            PC = mmu.read_byte(SP) | (mmu.read_byte(SP + 1) << 8);
            SP += 2;
            break;
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
        case 0xD9: // RETI
            PC = mmu.read_byte(SP) | (mmu.read_byte(SP + 1) << 8);
            SP += 2;
            ime = true; // Re-enable interrupts
            break;
    }
}

void CPU::execute_rotate_instructions(uint8_t opcode) {
    switch (opcode) {
        case 0x07: rlca(); break; // RLCA
        case 0x0F: rrca(); break; // RRCA
        case 0x17: rla(); break;  // RLA
        case 0x1F: rra(); break;  // RRA
    }
}

void CPU::execute_misc_instructions(uint8_t opcode) {
    switch (opcode) {
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

        case 0x2F: // CPL
            A = ~A;
            subtract_flag = true;
            half_carry_flag = true;
            break;

        case 0x37: // SCF
            carry_flag = true;
            subtract_flag = false;
            half_carry_flag = false;
            // zero_flag 不變
            break;

        case 0x3F: // CCF
            carry_flag = !carry_flag;
            subtract_flag = false;
            half_carry_flag = false;
            // zero_flag 不變
            break;

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
            }
            break;

        case 0xF3: // DI (Disable Interrupts)
            ime = false;
            break;
        case 0xFB: // EI (Enable Interrupts) - takes effect after next instruction
            ei_delay_pending = true;
            break;

        // RST instructions
        case 0xC7: SP -= 2; mmu.write_byte(SP, PC & 0xFF); mmu.write_byte(SP + 1, PC >> 8); PC = 0x00; break; // RST 00H
        case 0xCF: SP -= 2; mmu.write_byte(SP, PC & 0xFF); mmu.write_byte(SP + 1, PC >> 8); PC = 0x08; break; // RST 08H
        case 0xD7: SP -= 2; mmu.write_byte(SP, PC & 0xFF); mmu.write_byte(SP + 1, PC >> 8); PC = 0x10; break; // RST 10H
        case 0xDF: SP -= 2; mmu.write_byte(SP, PC & 0xFF); mmu.write_byte(SP + 1, PC >> 8); PC = 0x18; break; // RST 18H
        case 0xE7: SP -= 2; mmu.write_byte(SP, PC & 0xFF); mmu.write_byte(SP + 1, PC >> 8); PC = 0x20; break; // RST 20H
        case 0xEF: SP -= 2; mmu.write_byte(SP, PC & 0xFF); mmu.write_byte(SP + 1, PC >> 8); PC = 0x28; break; // RST 28H
        case 0xF7: SP -= 2; mmu.write_byte(SP, PC & 0xFF); mmu.write_byte(SP + 1, PC >> 8); PC = 0x30; break; // RST 30H
        case 0xFF: SP -= 2; mmu.write_byte(SP, PC & 0xFF); mmu.write_byte(SP + 1, PC >> 8); PC = 0x38; break; // RST 38H
    }
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

    // Dispatch to appropriate handler based on instruction type
    switch (opcode) {
        case 0x00: // NOP
            // No operation
            break;

        case 0x10: // STOP
            execute_stop_instruction();
            break;

        // Load instructions - delegate to existing function
        case 0x06: case 0x0E: case 0x16: case 0x1E: case 0x26: case 0x2E: case 0x36: case 0x3E:
        case 0x01: case 0x11: case 0x21: case 0x31:
        case 0x40: case 0x41: case 0x42: case 0x43: case 0x44: case 0x45: case 0x46: case 0x47:
        case 0x48: case 0x49: case 0x4A: case 0x4B: case 0x4C: case 0x4D: case 0x4E: case 0x4F:
        case 0x50: case 0x51: case 0x52: case 0x53: case 0x54: case 0x55: case 0x56: case 0x57:
        case 0x58: case 0x59: case 0x5A: case 0x5B: case 0x5C: case 0x5D: case 0x5E: case 0x5F:
        case 0x60: case 0x61: case 0x62: case 0x63: case 0x64: case 0x65: case 0x66: case 0x67:
        case 0x68: case 0x69: case 0x6A: case 0x6B: case 0x6C: case 0x6D: case 0x6E: case 0x6F:
        case 0x70: case 0x71: case 0x72: case 0x73: case 0x74: case 0x75: case 0x77:
        case 0x78: case 0x79: case 0x7A: case 0x7B: case 0x7C: case 0x7D: case 0x7E: case 0x7F:
        case 0x02: case 0x0A: case 0x12: case 0x1A:
        case 0xE0: case 0xE2: case 0xEA: case 0xF0: case 0xF2: case 0xF8: case 0xF9: case 0xFA:
        case 0x22: case 0x2A: case 0x32: case 0x3A:
        case 0x08:
            execute_load_instructions(opcode);
            break;

        // Arithmetic instructions
        case 0x09: case 0x19: case 0x29: case 0x39: // ADD HL, rr
        case 0x80: case 0x81: case 0x82: case 0x83: case 0x84: case 0x85: case 0x86: case 0x87: case 0xC6: // ADD
        case 0x88: case 0x89: case 0x8A: case 0x8B: case 0x8C: case 0x8D: case 0x8E: case 0x8F: case 0xCE: // ADC
        case 0x90: case 0x91: case 0x92: case 0x93: case 0x94: case 0x95: case 0x96: case 0x97: case 0xD6: // SUB
        case 0x98: case 0x99: case 0x9A: case 0x9B: case 0x9C: case 0x9D: case 0x9E: case 0x9F: case 0xDE: // SBC
        case 0xB8: case 0xB9: case 0xBA: case 0xBB: case 0xBC: case 0xBD: case 0xBE: case 0xBF: case 0xFE: // CP
        case 0xE8: // ADD SP, n
            execute_arithmetic_instructions(opcode);
            break;

        // Logical instructions
        case 0xA0: case 0xA1: case 0xA2: case 0xA3: case 0xA4: case 0xA5: case 0xA6: case 0xA7: case 0xE6: // AND
        case 0xB0: case 0xB1: case 0xB2: case 0xB3: case 0xB4: case 0xB5: case 0xB6: case 0xB7: case 0xF6: // OR
        case 0xA8: case 0xA9: case 0xAA: case 0xAB: case 0xAC: case 0xAD: case 0xAE: case 0xAF: case 0xEE: // XOR
            execute_logical_instructions(opcode);
            break;

        // Increment/Decrement instructions
        case 0x04: case 0x0C: case 0x14: case 0x1C: case 0x24: case 0x2C: case 0x34: case 0x3C: // INC
        case 0x05: case 0x0D: case 0x15: case 0x1D: case 0x25: case 0x2D: case 0x35: case 0x3D: // DEC
        case 0x03: case 0x13: case 0x23: case 0x33: // INC rr
        case 0x0B: case 0x1B: case 0x2B: case 0x3B: // DEC rr
            execute_inc_dec_instructions(opcode);
            break;

        // Stack operations
        case 0xC1: case 0xD1: case 0xE1: case 0xF1: // POP
        case 0xC5: case 0xD5: case 0xE5: case 0xF5: // PUSH
            execute_stack_instructions(opcode);
            break;

        // Jump instructions
        case 0xC3: case 0xC2: case 0xCA: case 0xD2: case 0xDA: case 0xE9: // JP
        case 0x18: case 0x20: case 0x28: case 0x30: case 0x38: // JR
            execute_jump_instructions(opcode);
            break;

        // Call/Return instructions
        case 0xCD: case 0xC4: case 0xCC: case 0xD4: case 0xDC: // CALL
        case 0xC9: case 0xC0: case 0xC8: case 0xD0: case 0xD8: case 0xD9: // RET
            execute_call_return_instructions(opcode);
            break;

        // Rotate/Shift instructions
        case 0x07: case 0x0F: case 0x17: case 0x1F: // Rotate accumulator
            execute_rotate_instructions(opcode);
            break;

        // Miscellaneous instructions
        case 0x27: case 0x2F: case 0x37: case 0x3F: // DAA, CPL, SCF, CCF
        case 0x76: // HALT
        case 0xF3: case 0xFB: // DI, EI
        case 0xC7: case 0xCF: case 0xD7: case 0xDF: case 0xE7: case 0xEF: case 0xF7: case 0xFF: // RST
            execute_misc_instructions(opcode);
            break;

        // Instruction not implemented yet
        default:
            if (log_file.is_open()) {
                log_file << "Instruction not implemented yet: 0x" << std::hex << (int)opcode << std::dec << std::endl;
            }
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
