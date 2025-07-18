// cycles.rs - CPU時脈週期常數定義
// 2025.07.18

pub type CyclesType = u32;

// 基本指令週期
pub const CYCLES_1: CyclesType = 4;
pub const CYCLES_2: CyclesType = 8;
pub const CYCLES_3: CyclesType = 12;
pub const CYCLES_4: CyclesType = 16;
pub const CYCLES_5: CyclesType = 20;

// 特殊指令週期
pub const CYCLES_CALL: CyclesType = 24; // CALL指令
pub const CYCLES_RET: CyclesType = 16; // RET指令
pub const CYCLES_JP: CyclesType = 16; // JP指令
pub const CYCLES_JR: CyclesType = 12; // JR指令

// 中斷相關週期
pub const CYCLES_INT: CyclesType = 20; // 中斷處理
pub const CYCLES_HALT: CyclesType = 4; // HALT指令
