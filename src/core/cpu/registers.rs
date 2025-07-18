use crate::core::cpu::instructions::register_utils::FlagOperations;
use crate::core::cpu::register_target::RegTarget;
use crate::core::error::{Error, HardwareError};
pub type Result<T> = std::result::Result<T, Error>;

#[derive(Debug)]
pub struct Registers {
    af: u16, // 累加器和標誌位
    bc: u16, // BC 寄存器對
    de: u16, // DE 寄存器對
    hl: u16, // HL 寄存器對
    sp: u16, // 堆疊指針
    pc: u16, // 程式計數器
}

impl FlagOperations for Registers {
    fn set_zero(&mut self, value: bool) {
        let flags = (self.af & 0xFF) as u8;
        let mut flags_struct = crate::core::cpu::flags::Flags::new(flags);
        flags_struct.set_zero(value);
        self.af = (self.af & 0xFF00) | (flags_struct.get() as u16);
    }

    fn set_subtract(&mut self, value: bool) {
        let flags = (self.af & 0xFF) as u8;
        let mut flags_struct = crate::core::cpu::flags::Flags::new(flags);
        flags_struct.set_subtract(value);
        self.af = (self.af & 0xFF00) | (flags_struct.get() as u16);
    }

    fn set_half_carry(&mut self, value: bool) {
        let flags = (self.af & 0xFF) as u8;
        let mut flags_struct = crate::core::cpu::flags::Flags::new(flags);
        flags_struct.set_half_carry(value);
        self.af = (self.af & 0xFF00) | (flags_struct.get() as u16);
    }

    fn set_carry(&mut self, value: bool) {
        let flags = (self.af & 0xFF) as u8;
        let mut flags_struct = crate::core::cpu::flags::Flags::new(flags);
        flags_struct.set_carry(value);
        self.af = (self.af & 0xFF00) | (flags_struct.get() as u16);
    }

    fn get_zero(&self) -> bool {
        let flags = (self.af & 0xFF) as u8;
        crate::core::cpu::flags::Flags::new(flags).zero()
    }

    fn get_carry(&self) -> bool {
        let flags = (self.af & 0xFF) as u8;
        crate::core::cpu::flags::Flags::new(flags).carry()
    }

    fn get_half_carry(&self) -> bool {
        let flags = (self.af & 0xFF) as u8;
        crate::core::cpu::flags::Flags::new(flags).half_carry()
    }

    fn get_subtract(&self) -> bool {
        let flags = (self.af & 0xFF) as u8;
        crate::core::cpu::flags::Flags::new(flags).subtract()
    }
}

impl Registers {
    pub fn new() -> Self {
        Self {
            af: 0,
            bc: 0,
            de: 0,
            hl: 0,
            sp: 0,
            pc: 0,
        }
    }

    pub fn reset(&mut self) -> Result<()> {
        self.set_pc(0x0100);
        self.set_af(0x01B0);
        self.set_bc(0x0013);
        self.set_de(0x00D8);
        self.set_hl(0x014D);
        self.set_sp(0xFFFE);
        Ok(())
    }

    // 取得寄存器數值
    pub fn get_pc(&self) -> u16 {
        self.pc
    }
    pub fn get_sp(&self) -> u16 {
        self.sp
    }
    pub fn get_af(&self) -> u16 {
        self.af
    }
    pub fn get_bc(&self) -> u16 {
        self.bc
    }
    pub fn get_de(&self) -> u16 {
        self.de
    }
    pub fn get_hl(&self) -> u16 {
        self.hl
    }

    // 設定寄存器數值
    pub fn set_pc(&mut self, value: u16) {
        self.pc = value;
    }
    pub fn set_sp(&mut self, value: u16) {
        self.sp = value;
    }
    pub fn set_af(&mut self, value: u16) {
        self.af = value & 0xFFF0;
    } // 最低4位始終為0
    pub fn set_bc(&mut self, value: u16) {
        self.bc = value;
    }
    pub fn set_de(&mut self, value: u16) {
        self.de = value;
    }
    pub fn set_hl(&mut self, value: u16) {
        self.hl = value;
    }

    // 8位元寄存器存取
    pub fn get_a(&self) -> u8 {
        (self.af >> 8) as u8
    }
    pub fn get_f(&self) -> u8 {
        (self.af & 0xFF) as u8
    }
    pub fn get_b(&self) -> u8 {
        (self.bc >> 8) as u8
    }
    pub fn get_c(&self) -> u8 {
        (self.bc & 0xFF) as u8
    }
    pub fn get_d(&self) -> u8 {
        (self.de >> 8) as u8
    }
    pub fn get_e(&self) -> u8 {
        (self.de & 0xFF) as u8
    }
    pub fn get_h(&self) -> u8 {
        (self.hl >> 8) as u8
    }
    pub fn get_l(&self) -> u8 {
        (self.hl & 0xFF) as u8
    }

    pub fn set_a(&mut self, value: u8) {
        self.af = (self.af & 0x00FF) | ((value as u16) << 8);
    }
    pub fn set_f(&mut self, value: u8) {
        self.af = (self.af & 0xFF00) | ((value & 0xF0) as u16);
    }
    pub fn set_b(&mut self, value: u8) {
        self.bc = (self.bc & 0x00FF) | ((value as u16) << 8);
    }
    pub fn set_c(&mut self, value: u8) {
        self.bc = (self.bc & 0xFF00) | (value as u16);
    }
    pub fn set_d(&mut self, value: u8) {
        self.de = (self.de & 0x00FF) | ((value as u16) << 8);
    }
    pub fn set_e(&mut self, value: u8) {
        self.de = (self.de & 0xFF00) | (value as u16);
    }
    pub fn set_h(&mut self, value: u8) {
        self.hl = (self.hl & 0x00FF) | ((value as u16) << 8);
    }
    pub fn set_l(&mut self, value: u8) {
        self.hl = (self.hl & 0xFF00) | (value as u16);
    }

    // 根據 RegTarget 存取寄存器
    pub fn get_register(&self, target: &RegTarget) -> Result<u8> {
        Ok(match target {
            RegTarget::A => self.get_a(),
            RegTarget::B => self.get_b(),
            RegTarget::C => self.get_c(),
            RegTarget::D => self.get_d(),
            RegTarget::E => self.get_e(),
            RegTarget::H => self.get_h(),
            RegTarget::L => self.get_l(),
            RegTarget::AF => self.get_a(),
            RegTarget::BC => self.get_b(),
            RegTarget::DE => self.get_d(),
            RegTarget::HL => self.get_h(),
            RegTarget::SP => (self.sp >> 8) as u8,
            RegTarget::PC => (self.pc >> 8) as u8,
            RegTarget::MemHL => return Err(Error::Hardware(HardwareError::InvalidRegisterAccess)),
        })
    }

    pub fn set_register(&mut self, target: &RegTarget, value: u8) -> Result<()> {
        match target {
            RegTarget::A => self.set_a(value),
            RegTarget::B => self.set_b(value),
            RegTarget::C => self.set_c(value),
            RegTarget::D => self.set_d(value),
            RegTarget::E => self.set_e(value),
            RegTarget::H => self.set_h(value),
            RegTarget::L => self.set_l(value),
            RegTarget::AF => self.set_a(value),
            RegTarget::BC => self.set_b(value),
            RegTarget::DE => self.set_d(value),
            RegTarget::HL => self.set_h(value),
            RegTarget::SP => self.sp = (self.sp & 0x00FF) | ((value as u16) << 8),
            RegTarget::PC => self.pc = (self.pc & 0x00FF) | ((value as u16) << 8),
            RegTarget::MemHL => return Err(Error::Hardware(HardwareError::InvalidRegisterAccess)),
        }
        Ok(())
    }
}
