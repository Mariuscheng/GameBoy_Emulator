use super::cpu::CPU;
use crate::core::cycles::CyclesType;
use crate::core::error::{Error, InstructionError, Result};

/// 計時器指令分派
pub fn dispatch(cpu: &mut CPU, opcode: u8) -> crate::core::error::Result<CyclesType> {
    match opcode {
        // TIMA/TMA/分頻器相關 stub
        0x04 => Ok(cpu.inc_tima()),
        0x05 => Ok(cpu.dec_tima()),
        0x06 => Ok(cpu.set_tma()),
        _ => Err(Error::Instruction(InstructionError::InvalidOpcode(opcode))),
    }
}

pub fn inc_tima(cpu: &mut CPU) -> CyclesType {
    cpu.tima = cpu.tima.wrapping_add(1);
    4
}
pub fn dec_tima(cpu: &mut CPU) -> CyclesType {
    cpu.tima = cpu.tima.wrapping_sub(1);
    4
}
pub fn set_tma(cpu: &mut CPU) -> CyclesType {
    cpu.tma = cpu.tima;
    4
}
