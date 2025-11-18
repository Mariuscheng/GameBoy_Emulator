# GameBoy CPU Flags Verification against Pandocs

## Current Test Failures
- 01:05 - 8-bit LD/memory instructions
- 02:04 - 16-bit LD instructions  
- 05:05 - Rotate/shift (RLCA, RRCA, RLA, RRA)
- 06:04 - Bit manipulation (BIT, SET, RES)
- 09:05 - 16-bit INC/DEC
- 10:04 - HALT & STOP
- 11:01 - Interrupts

## Possible Root Causes

1. **Zero Flag (Z)** not properly set in all arithmetic instructions
2. **Half-Carry Flag (H)** miscalculation in SUB/SBC/CP/DEC
3. **Subtract Flag (N)** not properly set after certain operations
4. **Carry Flag (C)** affected when it shouldn't be (e.g., INC/DEC)
5. **DAA** still has precision issues
6. **Memory operations** corrupting flags unexpectedly
7. **16-bit arithmetic** (ADD HL, INC HL, DEC HL) flag issues
8. **Interrupt handling** possibly corrupting flags

## Priority Fixes Needed
1. Verify all ALU zero_flag calculations (should check full 8-bit result, not just bit 7)
2. Verify borrow direction in SUB/SBC/DEC half_carry calculations
3. Check if any flag is being set when it shouldn't be
4. Verify DAA doesn't corrupt other flags inappropriately
