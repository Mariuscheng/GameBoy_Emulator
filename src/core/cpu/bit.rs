// CB 指令 stub function
use super::register_utils::{FlagOperations, RegTarget};
use crate::core::cpu::cpu::CPU;
use crate::core::cycles::{CYCLES_2, CYCLES_3, CyclesType};
use crate::core::error::{Error, InstructionError};

pub fn dispatch(cpu: &mut CPU, opcode: u8) -> crate::core::error::Result<CyclesType> {
    match opcode {
        0x00..=0x07 => {
            let reg = opcode & 0x07;
            let target = RegTarget::from_bits(reg)?;
            return Ok(cpu.rlc_r(target));
        }
        0x08..=0x0F => {
            let reg = opcode & 0x07;
            let target = RegTarget::from_bits(reg)?;
            return Ok(cpu.rrc_r(target));
        }
        0x10..=0x17 => {
            let reg = opcode & 0x07;
            let target = RegTarget::from_bits(reg)?;
            return Ok(cpu.rl_r(target));
        }
        0x18..=0x1F => {
            let reg = opcode & 0x07;
            let target = RegTarget::from_bits(reg)?;
            return Ok(cpu.rr_r(target));
        }
        0x20..=0x27 => {
            let reg = opcode & 0x07;
            let target = RegTarget::from_bits(reg)?;
            return Ok(cpu.sla_r(target));
        }
        0x28..=0x2F => {
            let reg = opcode & 0x07;
            let target = RegTarget::from_bits(reg)?;
            return Ok(cpu.sra_r(target));
        }
        0x30..=0x37 => {
            let reg = opcode & 0x07;
            let target = RegTarget::from_bits(reg)?;
            return Ok(cpu.swap_r(target));
        }
        0x38..=0x3F => {
            let reg = opcode & 0x07;
            let target = RegTarget::from_bits(reg)?;
            return Ok(cpu.srl_r(target));
        }
        0x40..=0x7F => {
            let bit = (opcode >> 3) & 0x07;
            let reg = opcode & 0x07;
            let target = RegTarget::from_bits(reg)?;
            return Ok(cpu.bit_b_r(bit, target));
        }
        0x80..=0xBF => {
            let bit = (opcode >> 3) & 0x07;
            let reg = opcode & 0x07;
            let target = RegTarget::from_bits(reg)?;
            return Ok(cpu.res_b_r(bit, target));
        }
        0xC0..=0xFF => {
            let bit = (opcode >> 3) & 0x07;
            let reg = opcode & 0x07;
            let target = RegTarget::from_bits(reg)?;
            return Ok(cpu.set_b_r(bit, target));
        }
        _ => return Err(Error::Instruction(InstructionError::InvalidOpcode(opcode))),
    }
    // ...existing code...

    //     let result = value >> 1;
    //     self.registers.set_zero(result == 0);
    //     self.registers.set_subtract(false);
    //     self.registers.set_half_carry(false);
    //     self.registers.set_carry(carry);
    //     self.set_reg_value(reg, result);
    //     if matches!(reg, RegTarget::HL) {
    //         CYCLES_3
    //     } else {
    //         CYCLES_2
    //     }
    // }
}
// --- CB 前綴指令 stub function ---
pub fn op_cb_10(cpu: &mut CPU) -> CyclesType {
    let b = cpu.registers.b;
    let carry_in = if cpu.registers.get_carry() { 1 } else { 0 };
    let result = (b << 1) | carry_in;
    cpu.registers.b = result;
    cpu.registers.set_zero(result == 0);
    cpu.registers.set_subtract(false);
    cpu.registers.set_half_carry(false);
    cpu.registers.set_carry((b & 0x80) != 0);
    2
}

pub fn op_cb_11(cpu: &mut CPU) -> CyclesType {
    let c = cpu.registers.c;
    let carry_in = if cpu.registers.get_carry() { 1 } else { 0 };
    let result = (c << 1) | carry_in;
    cpu.registers.c = result;
    cpu.registers.set_zero(result == 0);
    cpu.registers.set_subtract(false);
    cpu.registers.set_half_carry(false);
    cpu.registers.set_carry((c & 0x80) != 0);
    2
}

pub fn op_cb_1_a(cpu: &mut CPU) -> CyclesType {
    let d = cpu.registers.d;
    let carry_in = if cpu.registers.get_carry() { 0x80 } else { 0 };
    let result = (d >> 1) | carry_in;
    cpu.registers.d = result;
    cpu.registers.set_zero(result == 0);
    cpu.registers.set_subtract(false);
    cpu.registers.set_half_carry(false);
    cpu.registers.set_carry((d & 0x01) != 0);
    2
}

pub fn op_cb_23(cpu: &mut CPU) -> CyclesType {
    let e = cpu.registers.e;
    let result = e << 1;
    cpu.registers.e = result;
    cpu.registers.set_zero(result == 0);
    cpu.registers.set_subtract(false);
    cpu.registers.set_half_carry(false);
    cpu.registers.set_carry((e & 0x80) != 0);
    2
}

pub fn op_cb_2_c(cpu: &mut CPU) -> CyclesType {
    let h = cpu.registers.h;
    let result = (h >> 1) | (h & 0x80);
    cpu.registers.h = result;
    cpu.registers.set_zero(result == 0);
    cpu.registers.set_subtract(false);
    cpu.registers.set_half_carry(false);
    cpu.registers.set_carry((h & 0x01) != 0);
    2
}

pub fn op_cb_35(cpu: &mut CPU) -> CyclesType {
    let l = cpu.registers.l;
    let result = (l >> 4) | (l << 4);
    cpu.registers.l = result;
    cpu.registers.set_zero(result == 0);
    cpu.registers.set_subtract(false);
    cpu.registers.set_half_carry(false);
    cpu.registers.set_carry(false);
    2
}

pub fn op_cb_3_f(cpu: &mut CPU) -> CyclesType {
    let a = cpu.registers.a;
    let result = a >> 1;
    cpu.registers.a = result;
    cpu.registers.set_zero(result == 0);
    cpu.registers.set_subtract(false);
    cpu.registers.set_half_carry(false);
    cpu.registers.set_carry((a & 0x01) != 0);
    2
}

// BIT 指令 (CB 40~7F)
pub fn bit(cpu: &mut CPU, bit: u8, target: RegTarget) -> CyclesType {
    // TODO: 實作 BIT 指令
    8
}

// SET 指令 (CB C0~FF)
pub fn set(cpu: &mut CPU, bit: u8, target: RegTarget) -> CyclesType {
    // TODO: 實作 SET 指令
    8
}

// RES 指令 (CB 80~BF)

// RL 指令 (CB 10~17)
pub fn rl(cpu: &mut CPU, target: RegTarget) -> CyclesType {
    // TODO: 實作 RL 指令
    8
}

// RR 指令 (CB 18~1F)
pub fn rr(cpu: &mut CPU, target: RegTarget) -> CyclesType {
    // TODO: 實作 RR 指令
    8
}

// SLA 指令 (CB 20~27)
pub fn sla(cpu: &mut CPU, target: RegTarget) -> CyclesType {
    // TODO: 實作 SLA 指令
    8
}

// SRA 指令 (CB 28~2F)
pub fn sra(cpu: &mut CPU, target: RegTarget) -> CyclesType {
    // TODO: 實作 SRA 指令
    8
}

// SRL 指令 (CB 38~3F)
pub fn srl(cpu: &mut CPU, target: RegTarget) -> CyclesType {
    // TODO: 實作 SRL 指令
    8
}
