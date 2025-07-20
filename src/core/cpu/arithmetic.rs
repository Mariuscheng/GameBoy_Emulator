use super::cpu::CPU;
use super::register_utils::RegTarget;
use crate::core::cycles::CyclesType;
use crate::core::error::{Error, InstructionError, Result};

/// 算術運算指令分派
pub fn dispatch(cpu: &mut CPU, opcode: u8) -> crate::core::error::Result<CyclesType> {
    match opcode {
        // DEC r 指令族 (05, 0D, 15, 1D, 25, 2D, 35, 3D)
        0x05 | 0x0D | 0x15 | 0x1D | 0x25 | 0x2D | 0x35 | 0x3D => {
            let reg = (opcode >> 3) & 0x07;
            RegTarget::from_bits(reg).and_then(|target| Ok(cpu.dec_r(target)))
        }
        // INC r 指令族 (04, 0C, 14, 1C, 24, 2C, 34, 3C)
        0x04 | 0x0C | 0x14 | 0x1C | 0x24 | 0x2C | 0x34 | 0x3C => {
            let reg = (opcode >> 3) & 0x07;
            RegTarget::from_bits(reg).and_then(|target| Ok(cpu.inc_r(target)))
        }
        // ADD/ADC/SUB/SBC 指令族 (0x80-0x9F)
        0x80..=0x9F => {
            let src = (opcode & 0x07) as u8;
            let use_carry = (opcode & 0x18) == 0x18 || (opcode & 0x18) == 0x08;
            RegTarget::from_bits(src).and_then(|source| match opcode & 0xF0 {
                0x80 => Ok(cpu.add_a_r(source, false)),
                0x90 => Ok(cpu.sub_a_r(source, false)),
                _ => {
                    if use_carry {
                        if (opcode & 0xF0) == 0x80 {
                            Ok(cpu.add_a_r(source, true))
                        } else {
                            Ok(cpu.sub_a_r(source, true))
                        }
                    } else {
                        Err(Error::Instruction(InstructionError::InvalidOpcode(opcode)))
                    }
                }
            })
        }
        // 立即數運算指令 (0xC6, 0xCE, 0xD6, 0xDE)
        0xC6 | 0xCE | 0xD6 | 0xDE => {
            let use_carry = opcode == 0xCE || opcode == 0xDE;
            match opcode {
                0xC6 | 0xCE => Ok(cpu.add_a_n(use_carry)),
                0xD6 | 0xDE => Ok(cpu.sub_a_n(use_carry)),
                _ => Err(Error::Instruction(InstructionError::InvalidOpcode(opcode))),
            }
        }
        // INC (HL) 指令 (34)
        0x34 => Ok(cpu.inc_hl()),
        // DEC (HL) 指令 (35)
        0x35 => Ok(cpu.dec_hl()),
        // ADD HL,rr 指令 (09, 19, 29, 39)
        0x09 | 0x19 | 0x29 | 0x39 => {
            let reg = (opcode >> 3) & 0x03;
            RegTarget::from_bits(reg).and_then(|target| Ok(cpu.add_hl_rr(target)))
        }
        // ADD SP,n 指令 (E8)
        0xE8 => Ok(cpu.add_sp_n()),
        // SUB 指令 (97)
        0x97 => Ok(cpu.sub_a()),
        // SBC 指令 (9F)
        0x9F => Ok(cpu.sbc_a()),
        // AND 指令 (A0)
        0xA0 => cpu.and_a_r(RegTarget::A),
        // OR 指令 (B0)
        0xB0 => cpu.or_a_r(RegTarget::A),
        // XOR 指令 (A8)
        0xA8 => cpu.xor_a_r(RegTarget::A),
        // CP 指令 (B8)
        0xB8 => cpu.cp_a_r(RegTarget::A),
        _ => Err(Error::Instruction(InstructionError::InvalidOpcode(opcode))),
    }
}
