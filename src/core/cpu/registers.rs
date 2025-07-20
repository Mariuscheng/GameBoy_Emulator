use crate::core::cpu::flags::{Flag, Flags};
use crate::core::cpu::register_utils::FlagOperations;

#[derive(Debug, Default)]
pub struct Registers {
    pub a: u8,    // 累加器 A
    flags: Flags, // 標誌寄存器 F
    pub b: u8,    // B 寄存器
    pub c: u8,    // C 寄存器
    pub d: u8,    // D 寄存器
    pub e: u8,    // E 寄存器
    pub h: u8,    // H 寄存器
    pub l: u8,    // L 寄存器
    pub sp: u16,  // 堆疊指針
    pub pc: u16,  // 程式計數器
}

impl Registers {
    /// 設定 PC 暫存器
    pub fn set_pc(&mut self, value: u16) {
        self.pc = value;
    }
    /// 取得 HL 16位元暫存器
    pub fn get_hl(&self) -> u16 {
        ((self.h as u16) << 8) | (self.l as u16)
    }

    /// 設定 HL 16位元暫存器
    pub fn set_hl(&mut self, value: u16) {
        self.h = (value >> 8) as u8;
        self.l = value as u8;
    }

    /// 取得 PC 暫存器
    pub fn get_pc(&self) -> u16 {
        self.pc
    }

    /// 取得 AF 16位元暫存器
    pub fn get_af(&self) -> u16 {
        ((self.a as u16) << 8) | (self.get_f() as u16)
    }

    /// 取得 SP 暫存器
    pub fn get_sp(&self) -> u16 {
        self.sp
    }

    /// 取得 BC 16位元暫存器
    pub fn get_bc(&self) -> u16 {
        ((self.b as u16) << 8) | (self.c as u16)
    }

    /// 設定 BC 16位元暫存器
    pub fn set_bc(&mut self, value: u16) {
        self.b = (value >> 8) as u8;
        self.c = value as u8;
    }

    /// 取得 DE 16位元暫存器
    pub fn get_de(&self) -> u16 {
        ((self.d as u16) << 8) | (self.e as u16)
    }

    /// 設定 DE 16位元暫存器
    pub fn set_de(&mut self, value: u16) {
        self.d = (value >> 8) as u8;
        self.e = value as u8;
    }
    // 旗標操作
    pub fn set_flag(&mut self, flag: Flag, value: bool) {
        self.flags.set(flag as u8, value);
    }

    pub fn get_flag(&self, flag: Flag) -> bool {
        self.flags.get(flag as u8)
    }
    pub fn new() -> Self {
        Self {
            a: 0x01,
            flags: Flags::new(0xB0),
            b: 0x00,
            c: 0x13,
            d: 0x00,
            e: 0xD8,
            h: 0x01,
            l: 0x4D,
            sp: 0xFFFE,
            pc: 0x0100,
        }
    }

    pub fn set_sp(&mut self, value: u16) {
        self.sp = value;
    }

    // 8位寄存器的 getter/setter
    pub fn set_a(&mut self, value: u8) {
        println!("Registers::set_a called, value={:#b}", value);
        self.a = value;
    }

    pub fn get_c(&self) -> u8 {
        self.c
    }
    pub fn set_c(&mut self, value: u8) {
        self.c = value;
    }
    pub fn get_a(&self) -> u8 {
        self.a
    }

    pub fn get_b(&self) -> u8 {
        self.b
    }
    pub fn set_b(&mut self, value: u8) {
        self.b = value;
    }
    pub fn get_d(&self) -> u8 {
        self.d
    }
    pub fn set_d(&mut self, value: u8) {
        self.d = value;
    }

    pub fn get_e(&self) -> u8 {
        self.e
    }

    pub fn set_e(&mut self, value: u8) {
        self.e = value;
    }

    pub fn get_h(&self) -> u8 {
        self.h
    }

    pub fn set_h(&mut self, value: u8) {
        self.h = value;
    }

    pub fn get_l(&self) -> u8 {
        self.l
    }

    pub fn set_l(&mut self, value: u8) {
        self.l = value;
    }

    // 組合標誌操作
    pub fn update_flags(&mut self, z: bool, n: bool, h: bool, c: bool) {
        self.flags.set_zero(z);
        self.flags.set_subtract(n);
        self.flags.set_half_carry(h);
        self.flags.set_carry(c);
    }
    pub fn update_zero_and_carry(&mut self, z: bool, c: bool) {
        let n = self.get_subtract();
        let h = self.get_half_carry();
        self.update_flags(z, n, h, c);
    }

    // 取得標誌寄存器的值
    pub fn get_f(&self) -> u8 {
        self.flags.value()
    }
}

// --- GameBoy CPU 暫存器相關 stub function ---
pub fn set_zero_flag(registers: &mut Registers, value: bool) {
    registers.set_zero(value);
}
pub fn set_subtract_flag(registers: &mut Registers, value: bool) {
    registers.set_subtract(value);
}
pub fn set_half_carry_flag(registers: &mut Registers, value: bool) {
    registers.set_half_carry(value);
}
pub fn set_carry_flag(registers: &mut Registers, value: bool) {
    registers.set_carry(value);
}
pub fn get_zero_flag(registers: &Registers) -> bool {
    registers.get_zero()
}
pub fn get_subtract_flag(registers: &Registers) -> bool {
    registers.get_subtract()
}
pub fn get_half_carry_flag(registers: &Registers) -> bool {
    registers.get_half_carry()
}
pub fn get_carry_flag(registers: &Registers) -> bool {
    registers.get_carry()
}

/// CPU 標誌位處理 trait
impl FlagOperations for Registers {
    fn set_zero(&mut self, value: bool) {
        self.set_flag(Flag::Z, value);
    }

    fn set_subtract(&mut self, value: bool) {
        self.set_flag(Flag::N, value);
    }

    fn set_half_carry(&mut self, value: bool) {
        self.set_flag(Flag::H, value);
    }

    fn set_carry(&mut self, value: bool) {
        self.set_flag(Flag::C, value);
    }

    fn get_zero(&self) -> bool {
        self.get_flag(Flag::Z)
    }

    fn get_subtract(&self) -> bool {
        self.get_flag(Flag::N)
    }

    fn get_half_carry(&self) -> bool {
        self.get_flag(Flag::H)
    }

    fn get_carry(&self) -> bool {
        self.get_flag(Flag::C)
    }
}
