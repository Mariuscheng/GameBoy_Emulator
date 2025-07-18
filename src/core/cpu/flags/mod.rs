// flags/mod.rs - CPU 狀態標誌定義
#[derive(Debug, Clone, Copy)]
pub enum Flag {
    Z = 0x80, // Zero Flag (位元 7)
    N = 0x40, // Subtract Flag (位元 6)
    H = 0x20, // Half Carry Flag (位元 5)
    C = 0x10, // Carry Flag (位元 4)
}

#[derive(Debug, Clone, Copy)]
pub struct Flags(u8);

impl Default for Flags {
    fn default() -> Self {
        Flags(0)
    }
}

impl Flags {
    pub fn new(value: u8) -> Self {
        Flags(value)
    }

    pub fn get(&self) -> u8 {
        self.0
    }

    pub fn set(&mut self, value: u8) {
        self.0 = value;
    }

    pub fn get_flag(&self, flag: Flag) -> bool {
        self.0 & (flag as u8) != 0
    }

    pub fn set_flag(&mut self, flag: Flag, value: bool) {
        if value {
            self.0 |= flag as u8;
        } else {
            self.0 &= !(flag as u8);
        }
    }

    pub fn zero(&self) -> bool {
        self.get_flag(Flag::Z)
    }

    pub fn subtract(&self) -> bool {
        self.get_flag(Flag::N)
    }

    pub fn half_carry(&self) -> bool {
        self.get_flag(Flag::H)
    }

    pub fn carry(&self) -> bool {
        self.get_flag(Flag::C)
    }

    pub fn set_zero(&mut self, value: bool) {
        self.set_flag(Flag::Z, value)
    }

    pub fn set_subtract(&mut self, value: bool) {
        self.set_flag(Flag::N, value)
    }

    pub fn set_half_carry(&mut self, value: bool) {
        self.set_flag(Flag::H, value)
    }

    pub fn set_carry(&mut self, value: bool) {
        self.set_flag(Flag::C, value)
    }
}

pub trait FlagOperations {
    fn set_zero(&mut self, value: bool);
    fn set_subtract(&mut self, value: bool);
    fn set_half_carry(&mut self, value: bool);
    fn set_carry(&mut self, value: bool);
    fn get_zero(&self) -> bool;
    fn get_subtract(&self) -> bool;
    fn get_half_carry(&self) -> bool;
    fn get_carry(&self) -> bool;
}

impl FlagOperations for Flags {
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
