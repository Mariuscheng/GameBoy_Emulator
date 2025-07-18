// arithmetic.rs - CPU 算術運算指令
// 2025.07.18

use super::prelude::*;
use super::register_utils::RegisterOperations;

/// 算術運算指令分派
pub fn dispatch(cpu: &mut CPU, opcode: u8) -> Result<CyclesType> {
    match opcode {
        // INC r 指令族 (04, 0C, 14, 1C, 24, 2C, 34, 3C)
        0x04 | 0x0C | 0x14 | 0x1C | 0x24 | 0x2C | 0x34 | 0x3C => {
            let reg = (opcode >> 3) & 0x07;
            let target = RegTarget::from_bits(reg)?;
            let cycles = cpu.inc_r(target)?;
            cpu.increment_pc(1)?;
            Ok(cycles)
        }

        // DEC r 指令族 (05, 0D, 15, 1D, 25, 2D, 35, 3D)
        0x05 | 0x0D | 0x15 | 0x1D | 0x25 | 0x2D | 0x35 | 0x3D => {
            let reg = (opcode >> 3) & 0x07;
            let target = RegTarget::from_bits(reg)?;
            let cycles = cpu.dec_r(target)?;
            cpu.increment_pc(1)?;
            Ok(cycles)
        }

        // ADD/ADC/SUB/SBC 指令族 (0x80-0x9F)
        0x80..=0x9F => {
            let src = opcode & 0x07;
            let source = RegTarget::from_bits(src)?;
            let cycles = match opcode {
                0x80..=0x87 => cpu.add_a_r(source, false)?, // ADD A,r
                0x88..=0x8F => cpu.add_a_r(source, true)?,  // ADC A,r
                0x90..=0x97 => cpu.sub_a_r(source, false)?, // SUB A,r
                0x98..=0x9F => cpu.sub_a_r(source, true)?,  // SBC A,r
                _ => return Err(Error::Instruction(InstructionError::InvalidOpcode(opcode))),
            };
            cpu.increment_pc(1)?;
            Ok(cycles)
        }

        // 立即數運算指令
        0xC6 | 0xCE | 0xD6 | 0xDE => {
            let cycles = match opcode {
                0xC6 => cpu.add_a_n(false)?, // ADD A,n
                0xCE => cpu.add_a_n(true)?,  // ADC A,n
                0xD6 => cpu.sub_a_n(false)?, // SUB A,n
                0xDE => cpu.sub_a_n(true)?,  // SBC A,n
                _ => unreachable!(),
            };
            cpu.increment_pc(2)?;
            Ok(cycles)
        }

        // DAA - 十進制調整累加器
        0x27 => {
            let cycles = cpu.daa()?;
            cpu.increment_pc(1)?;
            Ok(cycles)
        }

        // 其他指令...
        _ => Err(Error::Instruction(InstructionError::InvalidOpcode(opcode))),
    }
}

pub trait ArithmeticInstructions {
    fn dec_r(&mut self, target: RegTarget) -> Result<CyclesType>;
    fn add_a_n(&mut self, use_carry: bool) -> Result<CyclesType>;
    fn sub_a_n(&mut self, use_carry: bool) -> Result<CyclesType>;
    fn add_a_r(&mut self, source: RegTarget, use_carry: bool) -> Result<CyclesType>;
    fn sub_a_r(&mut self, source: RegTarget, use_carry: bool) -> Result<CyclesType>;
    fn add_hl_rr(&mut self, source: RegTarget) -> Result<CyclesType>;
}

impl ArithmeticInstructions for CPU {
    fn dec_r(&mut self, target: RegTarget) -> Result<CyclesType> {
        let value = <Self as RegisterOperations>::get_register_value(self, &target)?;
        let result = value.wrapping_sub(1);

        // 設置標誌位
        let mut flags = self.registers.get_f() & 0x10; // 保留 C 標誌
        if result == 0 {
            flags |= 0x80;
        } // Z flag
        flags |= 0x40; // N flag 設置
        if (value & 0x0F) == 0 {
            flags |= 0x20;
        } // H flag

        self.registers.set_f(flags);
        <Self as RegisterOperations>::set_register_value(self, &target, result)?;

        Ok(if target == RegTarget::HL { 12 } else { 4 })
    }

    fn add_a_n(&mut self, use_carry: bool) -> Result<CyclesType> {
        let n = self.fetch_byte()?;
        let a = <Self as RegisterOperations>::get_register_value(self, &RegTarget::A)?;
        let carry = if use_carry && (self.registers.get_f() & 0x10) != 0 {
            1
        } else {
            0
        };

        let result = a.wrapping_add(n).wrapping_add(carry);

        let mut flags = 0;
        if result == 0 {
            flags |= 0x80;
        } // Z flag
        if ((a & 0x0F) + (n & 0x0F) + carry) > 0x0F {
            flags |= 0x20;
        } // H flag
        if (a as u16 + n as u16 + carry as u16) > 0xFF {
            flags |= 0x10;
        } // C flag

        self.registers.set_f(flags);
        self.registers.set_a(result);
        Ok(8)
    }

    fn sub_a_n(&mut self, use_carry: bool) -> Result<CyclesType> {
        let n = self.fetch_byte()?;
        let a = <Self as RegisterOperations>::get_register_value(self, &RegTarget::A)?;
        let carry = if use_carry && (self.registers.get_f() & 0x10) != 0 {
            1
        } else {
            0
        };

        let result = a.wrapping_sub(n).wrapping_sub(carry);

        let mut flags = 0x40; // Set N flag
        if result == 0 {
            flags |= 0x80;
        } // Z flag
        if (a & 0x0F) < ((n & 0x0F) + carry) {
            flags |= 0x20;
        } // H flag
        if (a as u16) < (n as u16 + carry as u16) {
            flags |= 0x10;
        } // C flag

        self.registers.set_f(flags);
        self.registers.set_a(result);
        self.increment_pc(1)?; // 遞增 PC (fetch_byte 已經遞增了一次)
        Ok(8)
    }

    fn add_a_r(&mut self, source: RegTarget, use_carry: bool) -> Result<CyclesType> {
        let n = <Self as RegisterOperations>::get_register_value(self, &source)?;
        let a = <Self as RegisterOperations>::get_register_value(self, &RegTarget::A)?;
        let carry = if use_carry && (self.registers.get_f() & 0x10) != 0 {
            1
        } else {
            0
        };

        let result = a.wrapping_add(n).wrapping_add(carry);

        let mut flags = 0;
        if result == 0 {
            flags |= 0x80;
        } // Z flag
        if ((a & 0x0F) + (n & 0x0F) + carry) > 0x0F {
            flags |= 0x20;
        } // H flag
        if (a as u16 + n as u16 + carry as u16) > 0xFF {
            flags |= 0x10;
        } // C flag

        self.registers.set_f(flags);
        <Self as RegisterOperations>::set_register_value(self, &RegTarget::A, result)?;
        Ok(if source == RegTarget::HL { 8 } else { 4 })
    }

    fn sub_a_r(&mut self, source: RegTarget, use_carry: bool) -> Result<CyclesType> {
        let n = <Self as RegisterOperations>::get_register_value(self, &source)?;
        let a = <Self as RegisterOperations>::get_register_value(self, &RegTarget::A)?;
        let carry = if use_carry && (self.registers.get_f() & 0x10) != 0 {
            1
        } else {
            0
        };

        let result = a.wrapping_sub(n).wrapping_sub(carry);

        let mut flags = 0x40; // Set N flag
        if result == 0 {
            flags |= 0x80;
        } // Z flag
        if (a & 0x0F) < ((n & 0x0F) + carry) {
            flags |= 0x20;
        } // H flag
        if (a as u16) < (n as u16 + carry as u16) {
            flags |= 0x10;
        } // C flag

        self.registers.set_f(flags);
        <Self as RegisterOperations>::set_register_value(self, &RegTarget::A, result)?;
        self.increment_pc(1)?; // 遞增 PC
        Ok(if source == RegTarget::HL { 8 } else { 4 })
    }

    fn add_hl_rr(&mut self, source: RegTarget) -> Result<CyclesType> {
        let hl = self.registers.get_hl();
        let source_value = match source {
            RegTarget::BC => self.registers.get_bc(),
            RegTarget::DE => self.registers.get_de(),
            RegTarget::HL => self.registers.get_hl(),
            _ => self.registers.get_sp(),
        };

        let (result, carry) = hl.overflowing_add(source_value);

        // 設置標誌位
        let mut flags = self.registers.get_f() & 0x80; // 保留 Z flag
        // N flag 設為 0
        flags &= !0x40;
        // 設置 H flag
        if ((hl & 0x0FFF) + (source_value & 0x0FFF)) > 0x0FFF {
            flags |= 0x20;
        }
        // 設置 C flag
        if carry {
            flags |= 0x10;
        }

        self.registers.set_f(flags);
        self.registers.set_hl(result);
        Ok(8)
    }
}
