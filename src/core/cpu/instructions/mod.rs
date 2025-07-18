pub mod arithmetic;
pub mod bit;
pub mod call;
pub mod control;
pub mod cycles;
pub mod jump;
pub mod load;
pub mod logic;
pub mod prelude;
pub mod register_utils;

use crate::core::error::{Error, InstructionError, Result};

pub fn execute(
    cpu: &mut crate::core::cpu::CPU,
    opcode: u8,
) -> Result<crate::core::cycles::CyclesType> {
    let pc = cpu.get_pc();

    // 非法操作碼判斷，但允許在啟動序列中的 Nintendo 標誌
    if [0xD3, 0xDB, 0xDD, 0xE3, 0xE4, 0xEB, 0xEC, 0xF4, 0xFC, 0xFD].contains(&opcode)
        || (opcode == 0xED && !(0x0104..=0x0133).contains(&pc))
    {
        return Err(Error::Instruction(InstructionError::InvalidOpcode(opcode)));
    }

    // 如果在 Nintendo 標誌區域，自動跳過
    if (0x0104..=0x0133).contains(&pc) {
        cpu.set_pc(0x0150);
        return Ok(4);
    }

    match opcode {
        // NOP
        0x00 => control::dispatch(cpu, opcode),

        // LD 指令
        0x08 => load::dispatch(cpu, opcode), // LD (nn),SP
        0x01 | 0x11 | 0x21 | 0x31 => load::dispatch(cpu, opcode), // LD rr,nn
        0x02 | 0x12 | 0x22 | 0x32 => load::dispatch(cpu, opcode), // LD (rr),A
        0x06 | 0x0E | 0x16 | 0x1E | 0x26 | 0x2E | 0x36 | 0x3E => load::dispatch(cpu, opcode), // LD r,n
        0x0A | 0x1A | 0x2A | 0x3A => load::dispatch(cpu, opcode), // LD A,(rr)
        0xEA | 0xFA | 0xE0 | 0xF0 | 0xE2 | 0xF2 => load::dispatch(cpu, opcode), // LD (nn),A, LD A,(nn), LDH
        0x40..=0x75 | 0x77..=0x7F => load::dispatch(cpu, opcode),               // LD r,r, LD (HL),r

        // INC/DEC 指令
        0x03 | 0x13 | 0x23 | 0x33 => arithmetic::dispatch(cpu, opcode), // INC rr
        0x0B | 0x1B | 0x2B | 0x3B => arithmetic::dispatch(cpu, opcode), // DEC rr
        0x04 | 0x0C | 0x14 | 0x1C | 0x24 | 0x2C | 0x34 | 0x3C => arithmetic::dispatch(cpu, opcode), // INC r
        0x05 | 0x0D | 0x15 | 0x1D | 0x25 | 0x2D | 0x35 | 0x3D => arithmetic::dispatch(cpu, opcode), // DEC r

        // CALL 和 RST 指令
        0xC4 | 0xCC | 0xCD | 0xD4 | 0xDC => match opcode {
            0xCD => call::call_nn(cpu),                                // CALL nn
            0xC4 => call::call_cc_nn(cpu, !cpu.registers.get_zero()),  // CALL NZ,nn
            0xCC => call::call_cc_nn(cpu, cpu.registers.get_zero()),   // CALL Z,nn
            0xD4 => call::call_cc_nn(cpu, !cpu.registers.get_carry()), // CALL NC,nn
            0xDC => call::call_cc_nn(cpu, cpu.registers.get_carry()),  // CALL C,nn
            _ => unreachable!(),
        },

        0xC7 | 0xCF | 0xD7 | 0xDF | 0xE7 | 0xEF | 0xF7 | 0xFF => match opcode {
            0xC7 => call::rst(cpu, 0x00), // RST 00H
            0xCF => call::rst(cpu, 0x08), // RST 08H
            0xD7 => call::rst(cpu, 0x10), // RST 10H
            0xDF => call::rst(cpu, 0x18), // RST 18H
            0xE7 => call::rst(cpu, 0x20), // RST 20H
            0xEF => call::rst(cpu, 0x28), // RST 28H
            0xF7 => call::rst(cpu, 0x30), // RST 30H
            0xFF => call::rst(cpu, 0x38), // RST 38H
            _ => unreachable!(),
        },

        // ADD/ADC/SUB/SBC/CP 指令
        0x80..=0x9F => arithmetic::dispatch(cpu, opcode), // ADD/ADC/SUB/SBC r
        0xC6 | 0xCE | 0xD6 | 0xDE => arithmetic::dispatch(cpu, opcode), // ADD/ADC/SUB/SBC n
        0xFE => logic::dispatch(cpu, opcode),             // CP n

        // 邏輯指令
        0xA0..=0xA7 => logic::dispatch(cpu, opcode), // AND r
        0xB0..=0xB7 => logic::dispatch(cpu, opcode), // OR r
        0xA8..=0xAF => logic::dispatch(cpu, opcode), // XOR r
        0xB8..=0xBF => logic::dispatch(cpu, opcode), // CP r

        // 旋轉和移位指令
        0x17 => {
            // RLA
            let a = cpu.get_a();
            let carry = if cpu.get_flag_c() { 1 } else { 0 };
            let new_carry = (a & 0x80) != 0;
            let result = (a << 1) | carry;
            cpu.set_a(result);
            cpu.set_flag_z(false);
            cpu.set_flag_n(false);
            cpu.set_flag_h(false);
            cpu.set_flag_c(new_carry);
            cpu.increment_pc(1)?;
            Ok(4)
        }

        // JR 條件跳轉指令
        0x20 | 0x28 | 0x30 | 0x38 => jump::dispatch(cpu, opcode), // JR cc,n

        // DAA - 十進制調整累加器
        0x27 => arithmetic::dispatch(cpu, opcode),

        // 其他控制指令
        0x76 => control::dispatch(cpu, opcode),        // HALT
        0xF8 | 0xF9 => load::dispatch(cpu, opcode),    // LD HL,SP+r8, LD SP,HL
        0xF3 | 0xFB => control::dispatch(cpu, opcode), // DI/EI
        0xC2 | 0xC3 | 0xCA | 0xD2 | 0xDA | 0xE9 => jump::dispatch(cpu, opcode), // JP
        0xC0 | 0xC8 | 0xC9 | 0xD0 | 0xD8 | 0xD9 => control::dispatch(cpu, opcode), // RET/RETI

        // CB 前綴指令
        0xCB => {
            let cb_opcode = cpu.fetch_byte()?;
            bit::dispatch(cpu, cb_opcode)
        }

        // 未實現的指令
        _ => Err(Error::Instruction(InstructionError::InvalidOpcode(opcode))),
    }
}

// Re-export important components
pub use prelude::FlagOperations;
