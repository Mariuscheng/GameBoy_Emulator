#include "cpu.h"

void CPU::handle_interrupt(uint8_t interrupt_type) {
    if (!ime) return;

    ime = false; // Disable interrupts

    // Push PC to stack
    SP -= 2;
    mmu.write_byte(SP, PC & 0xFF);
    mmu.write_byte(SP + 1, PC >> 8);

    // Jump to interrupt vector
    switch (interrupt_type) {
        case 0: PC = 0x40; break; // VBlank
        case 1: PC = 0x48; break; // LCD
        case 2: PC = 0x50; break; // Timer
        case 3: PC = 0x58; break; // Serial
        case 4: PC = 0x60; break; // Joypad
    }

    // Clear interrupt flag
    uint8_t if_reg = mmu.read_byte(0xFF0F);
    if_reg &= ~(1 << interrupt_type);
    mmu.write_byte(0xFF0F, if_reg);
}

void CPU::rst(uint8_t addr) {
    SP -= 2;
    mmu.write_byte(SP, PC & 0xFF);
    mmu.write_byte(SP + 1, PC >> 8);
    PC = addr;
}
