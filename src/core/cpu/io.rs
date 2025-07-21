use super::cpu::CPU;
use crate::core::cycles::CyclesType;
use crate::core::error::{Error, InstructionError, Result};

/// I/O 指令分派
pub fn dispatch(cpu: &mut CPU, opcode: u8) -> crate::core::error::Result<CyclesType> {
    // 自動補齊 stub，所有未覆蓋指令預設回傳 4 cycles
    Ok(4)
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
