use super::cpu::CPU;
use super::register_utils::RegTarget;
use crate::core::cycles::CyclesType;
use crate::core::error::{Error, InstructionError, Result};

/// 堆疊指令分派
pub fn dispatch(cpu: &mut CPU, opcode: u8) -> crate::core::error::Result<CyclesType> {
    match opcode {
        // PUSH 指令族 (C5, D5, E5, F5)
        0xC5 | 0xD5 | 0xE5 | 0xF5 => Ok(cpu.push_r(RegTarget::from_bits(opcode)?)),
        // POP 指令族 (C1, D1, E1, F1)
        0xC1 | 0xD1 | 0xE1 | 0xF1 => Ok(cpu.pop_r(RegTarget::from_bits(opcode)?)),
        _ => Err(Error::Instruction(InstructionError::InvalidOpcode(opcode))),
    }
}

// PUSH r stub
pub fn push_r(cpu: &mut CPU, reg: RegTarget) -> CyclesType {
    // TODO: 實作 PUSH r
    16
}
pub fn pop_r(cpu: &mut CPU, reg: RegTarget) -> CyclesType {
    // TODO: 實作 POP r
    12
}
