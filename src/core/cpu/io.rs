use super::cpu::CPU;
use crate::core::cycles::CyclesType;
use crate::core::error::{Error, InstructionError, Result};

/// I/O 指令分派
pub fn dispatch(cpu: &mut CPU, opcode: u8) -> crate::core::error::Result<CyclesType> {
    match opcode {
        // Joypad 相關 stub
        0xF0 => Ok(cpu.read_joypad()),
        0xE0 => Ok(cpu.write_joypad()),
        // Serial 相關 stub
        0xF1 => Ok(cpu.read_serial()),
        0xE1 => Ok(cpu.write_serial()),
        _ => Err(Error::Instruction(InstructionError::InvalidOpcode(opcode))),
    }
}

pub fn read_joypad(cpu: &mut CPU) -> CyclesType {
    // TODO: 實作 Joypad 讀取
    8
}
pub fn write_joypad(cpu: &mut CPU) -> CyclesType {
    // TODO: 實作 Joypad 寫入
    8
}
pub fn read_serial(cpu: &mut CPU) -> CyclesType {
    // TODO: 實作 Serial 讀取
    8
}
pub fn write_serial(cpu: &mut CPU) -> CyclesType {
    // TODO: 實作 Serial 寫入
    8
}
