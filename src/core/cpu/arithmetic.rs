/// DEC r: 指定暫存器減一
pub fn dec_r(cpu: &mut CPU, target: RegTarget) -> CyclesType {
    match target {
        RegTarget::A => cpu.registers.a = cpu.registers.a.wrapping_sub(1),
        RegTarget::B => cpu.registers.b = cpu.registers.b.wrapping_sub(1),
        RegTarget::C => cpu.registers.c = cpu.registers.c.wrapping_sub(1),
        RegTarget::D => cpu.registers.d = cpu.registers.d.wrapping_sub(1),
        RegTarget::E => cpu.registers.e = cpu.registers.e.wrapping_sub(1),
        RegTarget::H => cpu.registers.h = cpu.registers.h.wrapping_sub(1),
        RegTarget::L => cpu.registers.l = cpu.registers.l.wrapping_sub(1),
        RegTarget::HL => {
            let addr = cpu.registers.get_hl();
            let val = unsafe { &*cpu.mmu }
                .read_byte(addr)
                .unwrap_or(0)
                .wrapping_sub(1);
            let _ = unsafe { &mut *cpu.mmu }.write_byte(addr, val);
        }
        _ => {}
    }
    4
}

/// ADD A, r: A += 指定暫存器
pub fn add_a_r(cpu: &mut CPU, target: RegTarget, _use_carry: bool) -> CyclesType {
    let value = match target {
        RegTarget::A => cpu.registers.a,
        RegTarget::B => cpu.registers.b,
        RegTarget::C => cpu.registers.c,
        RegTarget::D => cpu.registers.d,
        RegTarget::E => cpu.registers.e,
        RegTarget::H => cpu.registers.h,
        RegTarget::L => cpu.registers.l,
        RegTarget::HL => {
            let addr = cpu.registers.get_hl();
            unsafe { &*cpu.mmu }.read_byte(addr).unwrap_or(0)
        }
        _ => 0,
    };
    cpu.registers.a = cpu.registers.a.wrapping_add(value);
    4
}

/// SUB A, r: A -= 指定暫存器
pub fn sub_a_r(cpu: &mut CPU, target: RegTarget, _use_carry: bool) -> CyclesType {
    let value = match target {
        RegTarget::A => cpu.registers.a,
        RegTarget::B => cpu.registers.b,
        RegTarget::C => cpu.registers.c,
        RegTarget::D => cpu.registers.d,
        RegTarget::E => cpu.registers.e,
        RegTarget::H => cpu.registers.h,
        RegTarget::L => cpu.registers.l,
        RegTarget::HL => {
            let addr = cpu.registers.get_hl();
            unsafe { &*cpu.mmu }.read_byte(addr).unwrap_or(0)
        }
        _ => 0,
    };
    cpu.registers.a = cpu.registers.a.wrapping_sub(value);
    4
}

/// ADD A, n: A += 立即值
pub fn add_a_n(cpu: &mut CPU, _use_carry: bool) -> CyclesType {
    let n = cpu.fetch_byte().unwrap_or(0);
    cpu.registers.a = cpu.registers.a.wrapping_add(n);
    4
}

/// SUB A, n: A -= 立即值
pub fn sub_a_n(cpu: &mut CPU, _use_carry: bool) -> CyclesType {
    let n = cpu.fetch_byte().unwrap_or(0);
    cpu.registers.a = cpu.registers.a.wrapping_sub(n);
    4
}
use super::cpu::CPU;
use super::register_utils::RegTarget;
use crate::core::cpu::register_utils::FlagOperations;
use crate::core::cycles::CyclesType;
use crate::core::error::{Error, InstructionError, Result};
use crate::core::utils::logger::log_to_file;

/// 算術運算指令分派
#[allow(dead_code)]
pub fn dispatch(cpu: &mut CPU, opcode: u8) -> crate::core::error::Result<CyclesType> {
    log_to_file(&format!(
        "[ARITHMETIC] dispatch: opcode={:02X}, PC={:04X}",
        opcode, cpu.registers.pc
    ));
    // INC r 指令族 (04, 0C, 14, 1C, 24, 2C, 34, 3C)
    if matches!(
        opcode,
        0x04 | 0x0C | 0x14 | 0x1C | 0x24 | 0x2C | 0x34 | 0x3C
    ) {
        let reg = (opcode >> 3) & 0x07;
        match reg {
            0 => {
                cpu.registers.b = cpu.registers.b.wrapping_add(1);
                cpu.registers.set_zero(cpu.registers.b == 0);
            }
            1 => {
                cpu.registers.c = cpu.registers.c.wrapping_add(1);
                cpu.registers.set_zero(cpu.registers.c == 0);
            }
            2 => {
                cpu.registers.d = cpu.registers.d.wrapping_add(1);
                cpu.registers.set_zero(cpu.registers.d == 0);
            }
            3 => {
                cpu.registers.e = cpu.registers.e.wrapping_add(1);
                cpu.registers.set_zero(cpu.registers.e == 0);
            }
            4 => {
                cpu.registers.h = cpu.registers.h.wrapping_add(1);
                cpu.registers.set_zero(cpu.registers.h == 0);
            }
            5 => {
                cpu.registers.l = cpu.registers.l.wrapping_add(1);
                cpu.registers.set_zero(cpu.registers.l == 0);
            }
            6 => {
                let addr = cpu.registers.get_hl();
                let val = unsafe { &*cpu.mmu }
                    .read_byte(addr)
                    .unwrap_or(0)
                    .wrapping_add(1);
                let _ = unsafe { &mut *cpu.mmu }.write_byte(addr, val);
                cpu.registers.set_zero(val == 0);
            }
            7 => {
                cpu.registers.a = cpu.registers.a.wrapping_add(1);
                cpu.registers.set_zero(cpu.registers.a == 0);
            }
            _ => {}
        }
        return Ok(4);
    }
    // DEC r 指令族 (05, 0D, 15, 1D, 25, 2D, 35, 3D)
    if matches!(
        opcode,
        0x05 | 0x0D | 0x15 | 0x1D | 0x25 | 0x2D | 0x35 | 0x3D
    ) {
        let reg = (opcode >> 3) & 0x07;
        match reg {
            0 => {
                cpu.registers.b = cpu.registers.b.wrapping_sub(1);
                cpu.registers.set_zero(cpu.registers.b == 0);
            }
            1 => {
                cpu.registers.c = cpu.registers.c.wrapping_sub(1);
                cpu.registers.set_zero(cpu.registers.c == 0);
            }
            2 => {
                cpu.registers.d = cpu.registers.d.wrapping_sub(1);
                cpu.registers.set_zero(cpu.registers.d == 0);
            }
            3 => {
                cpu.registers.e = cpu.registers.e.wrapping_sub(1);
                cpu.registers.set_zero(cpu.registers.e == 0);
            }
            4 => {
                cpu.registers.h = cpu.registers.h.wrapping_sub(1);
                cpu.registers.set_zero(cpu.registers.h == 0);
            }
            5 => {
                cpu.registers.l = cpu.registers.l.wrapping_sub(1);
                cpu.registers.set_zero(cpu.registers.l == 0);
            }
            6 => {
                let addr = cpu.registers.get_hl();
                let val = unsafe { &*cpu.mmu }
                    .read_byte(addr)
                    .unwrap_or(0)
                    .wrapping_sub(1);
                let _ = unsafe { &mut *cpu.mmu }.write_byte(addr, val);
                cpu.registers.set_zero(val == 0);
            }
            7 => {
                cpu.registers.a = cpu.registers.a.wrapping_sub(1);
                cpu.registers.set_zero(cpu.registers.a == 0);
            }
            _ => {}
        }
        return Ok(4);
    }
    // ADD A, r (80~87)
    if (opcode & 0xF8) == 0x80 {
        let src = opcode & 0x07;
        let value = match src {
            0 => cpu.registers.b,
            1 => cpu.registers.c,
            2 => cpu.registers.d,
            3 => cpu.registers.e,
            4 => cpu.registers.h,
            5 => cpu.registers.l,
            6 => {
                let addr = cpu.registers.get_hl();
                unsafe { &*cpu.mmu }.read_byte(addr).unwrap_or(0)
            }
            7 => cpu.registers.a,
            _ => 0,
        };
        cpu.registers.a = cpu.registers.a.wrapping_add(value);
        cpu.registers.set_zero(cpu.registers.a == 0);
        return Ok(4);
    }
    // SUB A, r (90~97)
    if (opcode & 0xF8) == 0x90 {
        let src = opcode & 0x07;
        let value = match src {
            0 => cpu.registers.b,
            1 => cpu.registers.c,
            2 => cpu.registers.d,
            3 => cpu.registers.e,
            4 => cpu.registers.h,
            5 => cpu.registers.l,
            6 => {
                let addr = cpu.registers.get_hl();
                unsafe { &*cpu.mmu }.read_byte(addr).unwrap_or(0)
            }
            7 => cpu.registers.a,
            _ => 0,
        };
        cpu.registers.a = cpu.registers.a.wrapping_sub(value);
        cpu.registers.set_zero(cpu.registers.a == 0);
        return Ok(4);
    }
    // AND A, r (A0~A7)
    if (opcode & 0xF8) == 0xA0 {
        let src = opcode & 0x07;
        let value = match src {
            0 => cpu.registers.b,
            1 => cpu.registers.c,
            2 => cpu.registers.d,
            3 => cpu.registers.e,
            4 => cpu.registers.h,
            5 => cpu.registers.l,
            6 => {
                let addr = cpu.registers.get_hl();
                unsafe { &*cpu.mmu }.read_byte(addr).unwrap_or(0)
            }
            7 => cpu.registers.a,
            _ => 0,
        };
        cpu.registers.a &= value;
        cpu.registers.set_zero(cpu.registers.a == 0);
        return Ok(4);
    }
    // OR A, r (B0~B7)
    if (opcode & 0xF8) == 0xB0 {
        let src = opcode & 0x07;
        let value = match src {
            0 => cpu.registers.b,
            1 => cpu.registers.c,
            2 => cpu.registers.d,
            3 => cpu.registers.e,
            4 => cpu.registers.h,
            5 => cpu.registers.l,
            6 => {
                let addr = cpu.registers.get_hl();
                unsafe { &*cpu.mmu }.read_byte(addr).unwrap_or(0)
            }
            7 => cpu.registers.a,
            _ => 0,
        };
        cpu.registers.a |= value;
        cpu.registers.set_zero(cpu.registers.a == 0);
        return Ok(4);
    }
    // XOR A, r (A8~AF)
    if (opcode & 0xF8) == 0xA8 {
        let src = opcode & 0x07;
        let value = match src {
            0 => cpu.registers.b,
            1 => cpu.registers.c,
            2 => cpu.registers.d,
            3 => cpu.registers.e,
            4 => cpu.registers.h,
            5 => cpu.registers.l,
            6 => {
                let addr = cpu.registers.get_hl();
                unsafe { &*cpu.mmu }.read_byte(addr).unwrap_or(0)
            }
            7 => cpu.registers.a,
            _ => 0,
        };
        cpu.registers.a ^= value;
        cpu.registers.set_zero(cpu.registers.a == 0);
        return Ok(4);
    }
    // CP A, r (B8~BF)
    if (opcode & 0xF8) == 0xB8 {
        let src = opcode & 0x07;
        let value = match src {
            0 => cpu.registers.b,
            1 => cpu.registers.c,
            2 => cpu.registers.d,
            3 => cpu.registers.e,
            4 => cpu.registers.h,
            5 => cpu.registers.l,
            6 => {
                let addr = cpu.registers.get_hl();
                unsafe { &*cpu.mmu }.read_byte(addr).unwrap_or(0)
            }
            7 => cpu.registers.a,
            _ => 0,
        };
        let result = cpu.registers.a.wrapping_sub(value);
        cpu.registers.set_zero(result == 0);
        return Ok(4);
    }
    Ok(4)
}
