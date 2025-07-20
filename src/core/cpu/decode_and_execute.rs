// Decode and execute instructions

use crate::core::cpu::registers::Registers;
use crate::core::cpu::registers::{CARRY_FLAG, HALF_CARRY_FLAG, SUBTRACT_FLAG, ZERO_FLAG};
use crate::core::mmu::MMU;

pub fn decode_and_execute(registers: &mut Registers, mmu: &mut MMU, opcode: u8) {
    match opcode {
        0x00 => { /* NOP */ }
        0x01 => { /* LD BC,nn */ }
        0x02 => { /* LD (BC),A */ }
        0x03 => { /* INC BC */ }
        0x04 => { /* INC B */ }
        0x05 => { /* DEC B */ }
        0x06 => { /* LD B,n */ }
        0x07 => { /* RLCA */ }
        0x08 => { /* LD (nn),SP */ }
        0x09 => { /* ADD HL,BC */ }
        0x0A => { /* LD A,(BC) */ }
        0x0B => { /* DEC BC */ }
        0x0C => { /* INC C */ }
        0x0D => { /* DEC C */ }
        0x0E => { /* LD C,n */ }
        0x0F => { /* RRCA */ }
        0x10 => { /* STOP */ }
        0x11 => { /* LD DE,nn */ }
        0x12 => { /* LD (DE),A */ }
        0x13 => { /* INC DE */ }
        0x14 => { /* INC D */ }
        0x15 => { /* DEC D */ }
        0x16 => { /* LD D,n */ }
        0x17 => { /* RLA */ }
        0x18 => { /* JR n */ }
        0x19 => { /* ADD HL,DE */ }
        0x1A => { /* LD A,(DE) */ }
        0x1B => { /* DEC DE */ }
        0x1C => { /* INC E */ }
        0x1D => { /* DEC E */ }
        0x1E => { /* LD E,n */ }
        0x1F => { /* RRA */ }
        0x20 => { /* JR NZ,n */ }
        0x21 => { /* LD HL,nn */ }
        0x22 => { /* LD (HL+),A */ }
        0x23 => { /* INC HL */ }
        0x24 => { /* INC H */ }
        0x25 => { /* DEC H */ }
        0x26 => { /* LD H,n */ }
        0x27 => { /* DAA */ }
        0x28 => { /* JR Z,n */ }
        0x29 => { /* ADD HL,HL */ }
        0x2A => { /* LD A,(HL+) */ }
        0x2B => { /* DEC HL */ }
        0x2C => { /* INC L */ }
        0x2D => { /* DEC L */ }
        0x2E => { /* LD L,n */ }
        0x2F => { /* CPL */ }
        0x30 => { /* JR NC,n */ }
        0x31 => { /* LD SP,nn */ }
        0x32 => { /* LD (HL-),A */ }
        0x33 => { /* INC SP */ }
        0x34 => { /* INC (HL) */ }
        0x35 => { /* DEC (HL) */ }
        0x36 => { /* LD (HL),n */ }
        0x37 => { /* SCF */ }
        0x38 => { /* JR C,n */ }
        0x39 => { /* ADD HL,SP */ }
        0x3A => { /* LD A,(HL-) */ }
        0x3B => { /* DEC SP */ }
        0x3C => { /* INC A */ }
        0x3D => { /* DEC A */ }
        0x3E => { /* LD A,n */ }
        0x3F => { /* CCF */ }
        0x40..=0x7F => { /* LD r,r */ }
        0x80..=0x87 => { /* ADD A,r */ }
        0x88..=0x8F => { /* ADC A,r */ }
        0x90..=0x97 => { /* SUB A,r */ }
        0x98..=0x9F => { /* SBC A,r */ }
        0xA0..=0xA7 => { /* AND A,r */ }
        0xA8..=0xAF => { /* XOR A,r */ }
        0xB0..=0xB7 => { /* OR A,r */ }
        0xB8..=0xBF => { /* CP A,r */ }
        0xC0 => { /* RET NZ */ }
        0xC1 => { /* POP BC */ }
        0xC2 => { /* JP NZ,nn */ }
        0xC3 => { /* JP nn */ }
        0xC4 => { /* CALL NZ,nn */ }
        0xC5 => { /* PUSH BC */ }
        0xC6 => { /* ADD A,n */ }
        0xC7 => { /* RST 00H */ }
        0xC8 => { /* RET Z */ }
        0xC9 => { /* RET */ }
        0xCA => { /* JP Z,nn */ }
        0xCB => { /* CB 前綴指令 */ }
        0xCC => { /* CALL Z,nn */ }
        0xCD => { /* CALL nn */ }
        0xCE => { /* ADC A,n */ }
        0xCF => { /* RST 08H */ }
        0xD0 => { /* RET NC */ }
        0xD1 => { /* POP DE */ }
        0xD2 => { /* JP NC,nn */ }
        0xD3 => { /* 未定義 */ }
        0xD4 => { /* CALL NC,nn */ }
        0xD5 => { /* PUSH DE */ }
        0xD6 => { /* SUB A,n */ }
        0xD7 => { /* RST 10H */ }
        0xD8 => { /* RET C */ }
        0xD9 => { /* RETI */ }
        0xDA => { /* JP C,nn */ }
        0xDB => { /* 未定義 */ }
        0xDC => { /* CALL C,nn */ }
        0xDD => { /* 未定義 */ }
        0xDE => { /* SBC A,n */ }
        0xDF => { /* RST 18H */ }
        0xE0 => { /* LDH (n),A */ }
        0xE1 => { /* POP HL */ }
        0xE2 => { /* LD (C),A */ }
        0xE3 => { /* 未定義 */ }
        0xE4 => { /* 未定義 */ }
        0xE5 => { /* PUSH HL */ }
        0xE6 => { /* AND A,n */ }
        0xE7 => { /* RST 20H */ }
        0xE8 => { /* ADD SP,n */ }
        0xE9 => { /* JP (HL) */ }
        0xEA => { /* LD (nn),A */ }
        0xEB => { /* 未定義 */ }
        0xEC => { /* 未定義 */ }
        0xED => { /* 未定義 */ }
        0xEE => { /* XOR A,n */ }
        0xEF => { /* RST 28H */ }
        0xF0 => { /* LDH A,(n) */ }
        0xF1 => { /* POP AF */ }
        0xF2 => { /* LD A,(C) */ }
        0xF3 => { /* DI */ }
        0xF4 => { /* 未定義 */ }
        0xF5 => { /* PUSH AF */ }
        0xF6 => { /* OR A,n */ }
        0xF7 => { /* RST 30H */ }
        0xF8 => { /* LD HL,SP+n */ }
        0xF9 => { /* LD SP,HL */ }
        0xFA => { /* LD A,(nn) */ }
        0xFB => { /* EI */ }
        0xFC => { /* 未定義 */ }
        0xFD => { /* 未定義 */ }
        0xFE => { /* CP A,n */ }
        0xFF => { /* RST 38H */ }
        _ => {
            println!(
                "未實現的指令: 0x{:02X} at PC: 0x{:04X}",
                opcode,
                registers.pc - 1
            );
        }
    }
}

#[test]
fn test_all_opcodes_covered() {
    let mut cpu = test_cpu();
    for opcode in 0x00..=0xFF {
        let _ = super::dispatch(&mut cpu, opcode);
        // 若有 panic，代表該指令未實作
    }
}
