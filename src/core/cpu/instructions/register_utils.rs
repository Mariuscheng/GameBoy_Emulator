use super::prelude::*;
use crate::core::cpu::CPU;
use crate::core::error::{Error, InstructionError};

pub trait RegisterOperations {
    fn get_register_value(&self, target: &RegTarget) -> Result<u8>;
    fn set_register_value(&mut self, target: &RegTarget, value: u8) -> Result<()>;
}

impl RegisterOperations for CPU {
    fn get_register_value(&self, target: &RegTarget) -> Result<u8> {
        match target {
            RegTarget::A => Ok(self.registers.get_a()),
            RegTarget::B => Ok(self.registers.get_b()),
            RegTarget::C => Ok(self.registers.get_c()),
            RegTarget::D => Ok(self.registers.get_d()),
            RegTarget::E => Ok(self.registers.get_e()),
            RegTarget::H => Ok(self.registers.get_h()),
            RegTarget::L => Ok(self.registers.get_l()),
            RegTarget::HL => Ok(self.mmu.borrow().read_byte(self.registers.get_hl())?),
            _ => Err(Error::Instruction(InstructionError::InvalidRegister(
                *target,
            ))),
        }
    }

    fn set_register_value(&mut self, target: &RegTarget, value: u8) -> Result<()> {
        match target {
            RegTarget::A => {
                self.registers.set_a(value);
                Ok(())
            }
            RegTarget::B => {
                self.registers.set_b(value);
                Ok(())
            }
            RegTarget::C => {
                self.registers.set_c(value);
                Ok(())
            }
            RegTarget::D => {
                self.registers.set_d(value);
                Ok(())
            }
            RegTarget::E => {
                self.registers.set_e(value);
                Ok(())
            }
            RegTarget::H => {
                self.registers.set_h(value);
                Ok(())
            }
            RegTarget::L => {
                self.registers.set_l(value);
                Ok(())
            }
            RegTarget::HL => {
                self.mmu
                    .borrow_mut()
                    .write_byte(self.registers.get_hl(), value)?;
                Ok(())
            }
            _ => Err(Error::Instruction(InstructionError::InvalidRegister(
                *target,
            ))),
        }
    }
}

// Convert register pair bits to register pair targets
pub fn get_reg_pair(bits: u8) -> Result<(RegTarget, RegTarget)> {
    let high_reg = match bits >> 4 {
        0x0 => RegTarget::B,
        0x1 => RegTarget::D,
        0x2 => RegTarget::H,
        0x3 => RegTarget::SP,
        _ => {
            return Err(Error::Instruction(InstructionError::InvalidRegister(
                RegTarget::B,
            )));
        }
    };

    let low_reg = match bits & 0x0F {
        0x0 => RegTarget::C,
        0x1 => RegTarget::E,
        0x2 => RegTarget::L,
        0x3 => RegTarget::SP,
        _ => {
            return Err(Error::Instruction(InstructionError::InvalidRegister(
                RegTarget::C,
            )));
        }
    };

    Ok((high_reg, low_reg))
}

// Convert bit pattern to single register target
pub fn get_reg_target(bits: u8) -> Result<RegTarget> {
    RegTarget::from_bits(bits)
}

// 標誌位操作 trait
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

// 計算 16 位元加法的進位
pub fn calc_16bit_carry(a: u16, b: u16, c: bool) -> bool {
    let c_in = if c { 1 } else { 0 };
    let result = (a as u32) + (b as u32) + (c_in as u32);
    result > 0xFFFF
}

// 計算 8 位元加法的半進位
pub fn calc_half_carry(a: u8, b: u8, c: bool) -> bool {
    let c_in = if c { 1 } else { 0 };
    let result = (a & 0x0F) + (b & 0x0F) + c_in;
    result > 0x0F
}

// 計算 16 位元加法的半進位
pub fn calc_16bit_half_carry(a: u16, b: u16) -> bool {
    ((a & 0x0FFF) + (b & 0x0FFF)) > 0x0FFF
}
