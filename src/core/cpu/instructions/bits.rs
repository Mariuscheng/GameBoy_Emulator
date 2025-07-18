// bits.rs - CPU 位操作指令（CB 前綴）
// 2025.07.18

use crate::core::cpu::CPU;
use crate::core::cycles::CyclesType;
use crate::core::cpu::register_target::RegTarget;
use crate::{Error, InstructionError, Result};

/// CB 前綴指令分派
pub fn dispatch(cpu: &mut CPU, opcode: u8) -> Result<CyclesType> {
    match opcode {
        // BIT b,r
        0x40..=0x7F => {
            let bit = (opcode >> 3) & 0x07;
            let reg = opcode & 0x07;
            let target = RegTarget::from_bits(reg)?;
            cpu.bit_b_r(bit, &target)
        }
        _ => Err(Error::Instruction(InstructionError::InvalidOpcode(opcode))),
    }
}

impl CPU {
    /// BIT b,r - 測試暫存器中的指定位元
    pub fn bit_b_r(&mut self, bit: u8, reg: &RegTarget) -> Result<CyclesType> {
        let value = match reg {
            RegTarget::A => self.registers.get_a(),
            RegTarget::B => self.registers.get_b(),
            RegTarget::C => self.registers.get_c(),
            RegTarget::D => self.registers.get_d(),
            RegTarget::E => self.registers.get_e(),
            RegTarget::H => self.registers.get_h(),
            RegTarget::L => self.registers.get_l(),
            RegTarget::HL => self.read_byte(self.registers.get_hl())?,
        };

        let bit_mask = 1 << bit;
        let result = (value & bit_mask) == 0;

        self.registers.set_flag_z(result);
        self.registers.set_flag_n(false);
        self.registers.set_flag_h(true);

        Ok(if reg == RegTarget::HL { 12 } else { 8 })
    }
}
