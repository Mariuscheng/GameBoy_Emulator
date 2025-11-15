#ifndef CPU_H
#define CPU_H

#include <cstdint>
#include "mmu.h"
#include <fstream>

class CPU {
public:
    CPU(MMU& mmu);
    ~CPU();

    // Registers
    union {
        struct {
            uint8_t F; // Flags
            uint8_t A; // Accumulator
        };
        uint16_t AF;
    };
    union {
        struct {
            uint8_t C;
            uint8_t B;
        };
        uint16_t BC;
    };
    union {
        struct {
            uint8_t E;
            uint8_t D;
        };
        uint16_t DE;
    };
    union {
        struct {
            uint8_t L;
            uint8_t H;
        };
        uint16_t HL;
    };
    uint16_t SP; // Stack Pointer
    uint16_t PC; // Program Counter

    // Flags
    bool zero_flag;
    bool subtract_flag;
    bool half_carry_flag;
    bool carry_flag;

    // Interrupt master enable
    bool ime;

    // EI instruction delay - EI takes effect after the next instruction
    bool ei_delay_pending;

    // CPU state
    bool halted;
    bool just_woken_from_halt; // Flag to handle HALT bug interrupt processing
    bool halt_bug_active; // Triggers modified fetch (PC not incremented) after HALT bug condition

    // Debug counter
    uint64_t step_count;
    // HALT debug counters
    uint32_t halt_count;        // Number of HALT instructions executed
    uint32_t halt_bug_count;    // Number of HALT bug triggers (one-shot fetch glitches)

    // Methods
    void reset();
    int step(); // Returns number of cycles executed
    void handle_interrupt(uint8_t interrupt_type);
    void execute_instruction(uint8_t opcode);
    int execute_instruction_with_cycles(uint8_t opcode); // Returns cycles
    void sync_f_register(); // Sync F register from flags
    void load_flags_from_f(); // Load flags from F register
    
    // Timing test quick mode
    void set_timing_test_mode(bool on) { timing_test_mode = on; }

private:
    MMU& mmu;
    std::ofstream log_file;
    // Instruction cycle instrumentation log (used for instr_timing analysis)
    std::ofstream instr_cycle_log;

    // Helper functions
    void add(uint8_t value);
    void sub(uint8_t value);
    void and_op(uint8_t value);
    void or_op(uint8_t value);
    void xor_op(uint8_t value);
    void adc(uint8_t value);
    void sbc(uint8_t value);
    void cp(uint8_t value);
    void inc(uint8_t& reg);
    void dec(uint8_t& reg);
    void rlca();
    void rrca();
    void rla();
    void rra();
    void rst(uint8_t addr);
    void add_hl(uint16_t value);
    void execute_cb_instruction(uint8_t cb_opcode);
    uint8_t& get_register_ref(uint8_t reg_code);
    void rlc(uint8_t& reg);
    void rrc(uint8_t& reg);
    void rl(uint8_t& reg);
    void rr(uint8_t& reg);
    void sla(uint8_t& reg);
    void sra(uint8_t& reg);
    void swap(uint8_t& reg);
    void srl(uint8_t& reg);
    void bit(uint8_t bit, uint8_t value);
    void res(uint8_t bit, uint8_t& reg);
    void set(uint8_t bit, uint8_t& reg);

    // --- Quick micro-step helpers (Route A) ---
    bool timing_test_mode = false;
    int timing_burned_tcycles = 0; // consumed inside instruction
    void burn_tcycles(int t) {
        if (!timing_test_mode || t <= 0) return;
        for (int i = 0; i < t; ++i) {
            mmu.get_ppu().step(1, mmu);
            mmu.get_apu().step(1);
            mmu.update_timer_cycles(1);
            ++timing_burned_tcycles;
        }
    }
    void burn_align4_then(int extra) {
        if (!timing_test_mode) return;
        // Align based on PPU phase to stabilize read timing test
        uint8_t mod4 = mmu.get_ppu().get_cycle_mod4();
        int need = (4 - (mod4 % 4)) % 4;
        if (need) burn_tcycles(need);
        if (extra > 0) burn_tcycles(extra);
    }
};

#endif // CPU_H