// Bus APU 相關欄位與邏輯
use crate::interface::audio::SimpleAPUSynth;
use std::sync::{Arc, Mutex};

pub struct BusAPU {
    pub apu_dbg_printed: bool,
    pub apu_regs: [u8; 0x30],
    pub apu_synth: Option<Arc<Mutex<SimpleAPUSynth>>>,
    pub dbg_timer: bool,
}

impl BusAPU {
    pub fn new() -> Self {
        Self {
            apu_dbg_printed: false,
            apu_regs: [0; 0x30],
            apu_synth: None,
            dbg_timer: std::env::var("GB_DEBUG_TIMER")
                .ok()
                .map(|v| v != "0")
                .unwrap_or(false),
        }
    }
    pub fn attach_synth(
        &mut self,
        synth: std::sync::Arc<std::sync::Mutex<crate::interface::audio::SimpleAPUSynth>>,
    ) {
        self.apu_synth = Some(synth);
        // 可於此呼叫 apu_update_synth(true) 等初始化音源狀態
    }
}
