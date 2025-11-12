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

    // Methods
    void reset();
    int step(); // Returns number of cycles executed
    void handle_interrupt(uint8_t interrupt_type);
    void execute_instruction(uint8_t opcode);
    int execute_instruction_with_cycles(uint8_t opcode); // Returns cycles
    void sync_f_register(); // Sync F register from flags
    void load_flags_from_f(); // Load flags from F register

private:
    MMU& mmu;
    std::ofstream log_file;

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
};

#endif // CPU_H