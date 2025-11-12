#include "cpu.h"

void CPU::bit(uint8_t bit, uint8_t value) {
    // BIT n, r: Test bit n in register r
    // Z = 1 if bit is 0, N = 0, H = 1, C not affected
    zero_flag = (value & (1 << bit)) == 0;
    subtract_flag = false;
    half_carry_flag = true;
    // carry_flag remains unchanged
}

void CPU::res(uint8_t bit, uint8_t& reg) {
    // RES n, r: Reset bit n (clear to 0)
    // No flags affected
    reg &= ~(1 << bit);
}

void CPU::set(uint8_t bit, uint8_t& reg) {
    // SET n, r: Set bit n (set to 1)
    // No flags affected
    reg |= (1 << bit);
}
