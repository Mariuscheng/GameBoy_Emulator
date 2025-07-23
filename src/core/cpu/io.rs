use super::cpu::CPU;
use crate::core::cycles::CyclesType;
use crate::core::error::{Error, InstructionError, Result};

/// I/O 指令分派
pub fn dispatch(cpu: &mut CPU, opcode: u8) -> crate::core::error::Result<CyclesType> {
    // 自動補齊 stub，所有未覆蓋指令預設回傳 4 cycles
    Ok(4)
}

pub fn read_joypad(cpu: &mut CPU) -> CyclesType {
    // 取得 Joypad 狀態（由 SDL3 backend 注入，需 event_pump）
    let state = if let (Some(backend), Some(event_pump)) = (cpu.sdl_backend, cpu.sdl_event_pump) {
        unsafe { crate::interface::sdl3_display::SdlDisplayBackend::get_joypad_state(&*event_pump) }
    } else {
        crate::interface::input::joypad::JoypadState::default()
    };
    // Game Boy Joypad register 0xFF00: 0=按下, 1=未按
    // bit 7-6 unused, bit 5 select button, bit 4 select dpad
    // bit 3-0: Down, Up, Left, Right, Start, Select, B, A
    let mut value = 0xFF;
    // TODO: 根據 select button/dpad 實作
    // 這裡僅示意，實際需根據 0xFF00 select bit
    if state.start {
        value &= !(1 << 3);
    }
    if state.select {
        value &= !(1 << 2);
    }
    if state.b {
        value &= !(1 << 1);
    }
    if state.a {
        value &= !(1 << 0);
    }
    if state.down {
        value &= !(1 << 3);
    }
    if state.up {
        value &= !(1 << 2);
    }
    if state.left {
        value &= !(1 << 1);
    }
    if state.right {
        value &= !(1 << 0);
    }
    // TODO: 實際應根據 0xFF00 select bit 分流
    unsafe {
        cpu.mmu.as_mut().unwrap().io[0x00] = value;
    }
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
