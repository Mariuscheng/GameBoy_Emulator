use super::cpu::CPU;
use crate::core::cycles::CyclesType;
use crate::core::error::{Error, InstructionError, Result};

/// 未公開/特殊指令分派
pub fn dispatch(cpu: &mut CPU, opcode: u8) -> crate::core::error::Result<CyclesType> {
    match opcode {
        // SGB 指令範例 (如 0xFC)
        0xFC => Ok(undocumented_sgb(cpu)),
        // 未定義指令範例 (如 0xDD, 0xED, 0xFD)
        0xDD | 0xED | 0xFD => Ok(undocumented_nop(cpu)),
        // HALT bug 指令 (如 0x76)
        0x76 => Ok(undocumented_halt_bug(cpu)),
        _ => Err(Error::Instruction(InstructionError::InvalidOpcode(opcode))),
    }
}

pub fn undocumented_sgb(cpu: &mut CPU) -> CyclesType {
    // TODO: 實作 SGB 指令
    4
}

pub fn undocumented_nop(cpu: &mut CPU) -> CyclesType {
    // TODO: 實作未定義 NOP
    4
}

pub fn undocumented_halt_bug(cpu: &mut CPU) -> CyclesType {
    // TODO: 實作 HALT bug 行為
    4
}
