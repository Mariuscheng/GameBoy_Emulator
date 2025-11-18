#include "cpu.h"
#include <iostream>

void CPU::sync_f_register() {
    // Update F register from individual flag bits
    F = (zero_flag ? 0x80 : 0) |
        (subtract_flag ? 0x40 : 0) |
        (half_carry_flag ? 0x20 : 0) |
        (carry_flag ? 0x10 : 0);
}

void CPU::load_flags_from_f() {
    // Load individual flags from F register
    zero_flag = (F & 0x80) != 0;
    subtract_flag = (F & 0x40) != 0;
    half_carry_flag = (F & 0x20) != 0;
    carry_flag = (F & 0x10) != 0;
}
