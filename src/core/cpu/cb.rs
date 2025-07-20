use super::cpu::CPU;
use super::register_utils::RegTarget;
use crate::core::cycles::CyclesType;
use crate::core::error::{Error, InstructionError, Result};

/// CB 前綴指令分派
pub fn dispatch(cpu: &mut CPU, opcode: u8) -> crate::core::error::Result<CyclesType> {
    match opcode {
        // BIT 指令族 (CB 40~7F)
        0x40..=0x7F => Ok(cpu.bit_b_r(0, RegTarget::from_bits(opcode)?)),
        // SET 指令族 (CB C0~FF)
        0xC0..=0xFF => Ok(cpu.set_b_r(0, RegTarget::from_bits(opcode)?)),
        // RES 指令族 (CB 80~BF)
        0x80..=0xBF => Ok(cpu.res_b_r(0, RegTarget::from_bits(opcode)?)),
        // RL/RR/SLA/SRA/SRL/SWAP stub
        0x10..=0x3F => Ok(cpu.cb_misc(RegTarget::from_bits(opcode)?)),
        _ => Err(Error::Instruction(InstructionError::InvalidOpcode(opcode))),
    }
}

pub fn bit(cpu: &mut CPU, reg: RegTarget) -> CyclesType {
    // TODO: 實作 BIT 指令
    8
}
pub fn set(cpu: &mut CPU, reg: RegTarget) -> CyclesType {
    // TODO: 實作 SET 指令
    8
}
pub fn res(cpu: &mut CPU, reg: RegTarget) -> CyclesType {
    // TODO: 實作 RES 指令
    8
}
pub fn cb_misc(cpu: &mut CPU, reg: RegTarget) -> CyclesType {
    // TODO: 實作 RL/RR/SLA/SRA/SRL/SWAP
    8
}
