#include "cpu.h"

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
