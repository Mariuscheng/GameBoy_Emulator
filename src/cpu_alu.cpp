#include "cpu.h"

void CPU::add(uint8_t value) {
    uint16_t result = A + value;
    zero_flag = ((result & 0xFF) == 0);
    subtract_flag = false;
    half_carry_flag = ((A & 0xF) + (value & 0xF)) > 0xF;
    carry_flag = result > 0xFF;
    A = static_cast<uint8_t>(result);
}

void CPU::sub(uint8_t value) {
    uint16_t result = A - value;
    zero_flag = ((result & 0xFF) == 0);
    subtract_flag = true;
    // H flag: set if borrow from bit 4 (lower nibble underflows)
    half_carry_flag = (A & 0x0F) < (value & 0x0F);
    carry_flag = (A < value); // borrow from bit 8
    A = static_cast<uint8_t>(result);
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
    uint8_t carry = carry_flag ? 1 : 0;
    uint16_t result = A + value + carry;
    zero_flag = ((result & 0xFF) == 0);
    subtract_flag = false;
    half_carry_flag = ((A & 0xF) + (value & 0xF) + carry) > 0xF;
    carry_flag = result > 0xFF;
    A = static_cast<uint8_t>(result);
}

void CPU::sbc(uint8_t value) {
    uint8_t carry = carry_flag ? 1 : 0;
    uint16_t result = A - value - carry;
    zero_flag = ((result & 0xFF) == 0);
    subtract_flag = true;
    // H flag: set if borrow from bit 4 (lower nibble underflows)
    // Borrow occurs if (A & 0x0F) < ((value & 0x0F) + carry)
    uint16_t a_low = A & 0x0F;
    uint16_t v_low = value & 0x0F;
    half_carry_flag = (a_low < (v_low + carry));
    carry_flag = ((uint16_t)A < (uint16_t)value + (uint16_t)carry);
    A = static_cast<uint8_t>(result);
}

void CPU::cp(uint8_t value) {
    // CP A, n: Compare A with n (A - n, but A not modified)
    uint16_t result = A - value;
    zero_flag = ((result & 0xFF) == 0);
    subtract_flag = true;
    // H flag: set if borrow from bit 4 (lower nibble underflows)
    half_carry_flag = (A & 0x0F) < (value & 0x0F);
    carry_flag = (A < value); // borrow from bit 8
    // A is not modified
}

void CPU::inc(uint8_t& reg) {
    uint8_t old = reg;
    reg++;
    zero_flag = (reg == 0);
    subtract_flag = false;
    half_carry_flag = ((old & 0xF) + 1) > 0xF; // set if lower nibble overflows
}

void CPU::dec(uint8_t& reg) {
    uint8_t old = reg;
    reg--;
    zero_flag = (reg == 0);
    subtract_flag = true;
    // H flag: set if borrow from bit 4 when decrementing
    half_carry_flag = ((old & 0x0F) == 0); // H is set if lower nibble was 0 (requires borrow)
}

void CPU::add_hl(uint16_t value) {
    uint32_t result = HL + value;
    subtract_flag = false;
    half_carry_flag = ((HL & 0x0FFF) + (value & 0x0FFF)) > 0x0FFF;
    carry_flag = result > 0xFFFF;
    HL = result & 0xFFFF;
}
