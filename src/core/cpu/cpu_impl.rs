use crate::core::cpu::instructions::control::ControlInstructions;
use crate::core::cycles::*;
use crate::core::mmu::MMU;
use crate::core::registers::Registers;
use crate::{Error, HardwareError, Result};
use crate::core::cpu::register_target::RegTarget;
use std::{cell::RefCell, rc::Rc};

// 定義週期常量
const CYCLES_4: CyclesType = 4;
const CYCLES_8: CyclesType = 8;
const CYCLES_12: CyclesType = 12;
const CYCLES_16: CyclesType = 16;
const CYCLES_20: CyclesType = 20;
const CYCLES_24: CyclesType = 24;

#[derive(Debug)]
pub struct CPU {
    pub registers: Registers,
    pub mmu: Rc<RefCell<MMU>>,
    pub halted: bool,
    pub ime: bool,
    pub ime_scheduled: bool,
    pub instruction_count: u64,
}

impl CPU {
    pub fn new(mmu: Rc<RefCell<MMU>>) -> Self {
        std::fs::create_dir_all("logs").ok();
        let mut cpu = Self {
            registers: Registers::new(),
            mmu,
            halted: false,
            ime: false,
            ime_scheduled: false,
            instruction_count: 0,
        };

        // 初始化寄存器為 DMG 的開機值
        cpu.registers.set_pc(0x0100); // ROM 入口點
        cpu.registers.set_af(0x01B0); // A=0x01, F=0xB0 (Z和H標誌設置)
        cpu.registers.set_bc(0x0013); // B=0x00, C=0x13
        cpu.registers.set_de(0x00D8); // D=0x00, E=0xD8
        cpu.registers.set_hl(0x014D); // H=0x01, L=0x4D
        cpu.registers.set_sp(0xFFFE); // 初始堆疊指針

        cpu
    }

    pub fn decode_and_execute(&mut self, opcode: u8) -> Result<CyclesType> {
        self.instruction_count += 1;

        match opcode {
            // 控制指令
            0x00 => {
                // 執行NOP指令
                log::debug!(
                    "執行 NOP 指令 在 PC=0x{:04X}",
                    self.registers.get_pc().wrapping_sub(1)
                );
                // NOP不做任何事，但PC需要遞增
                self.increment_pc(1)?;
                Ok(CYCLES_4)
            }
            0x76 => self.halt(),
            0x10 => self.stop(),
            0xFB => self.enable_interrupts(),
            0xF3 => self.disable_interrupts(),
            0xC3 => self.jump(), // JP nn
            0xCD => self.call(),
            0xC4 | 0xCC | 0xD4 | 0xDC => self.call_conditional(opcode),
            0xC9 => self.ret(),
            0xC0 | 0xC8 | 0xD0 | 0xD8 => self.ret_conditional(opcode),
            0xD9 => self.reti(),
            // ... 其他指令保持不變
            _ => {
                log::warn!(
                    "未知的指令：0x{:02X} 在 PC=0x{:04X}",
                    opcode,
                    self.registers.get_pc().wrapping_sub(1)
                );
                Ok(CYCLES_4)
            }
        }
    }

    pub fn dec_r(&mut self, target: RegTarget) -> Result<CyclesType> {
        let value = self.registers.get_register(target)?;
        let result = value.wrapping_sub(1);
        self.registers.set_register(target, result)?;

        self.registers.set_zero(result == 0);
        self.registers.set_subtract(true);
        self.registers.set_half_carry((value & 0x0F) == 0);

        Ok(CYCLES_4)
    }

    pub fn add_a_r(&mut self, source: RegTarget, use_carry: bool) -> Result<CyclesType> {
        let a = self.registers.get_a();
        let value = self.registers.get_register(source)?;
        let carry = if use_carry && self.registers.get_carry() {
            1
        } else {
            0
        };

        let result = a.wrapping_add(value).wrapping_add(carry);
        self.registers.set_a(result);

        self.registers.set_zero(result == 0);
        self.registers.set_subtract(false);
        self.registers
            .set_half_carry(((a & 0x0F) + (value & 0x0F) + carry) > 0x0F);
        self.registers
            .set_carry(((a as u16) + (value as u16) + (carry as u16)) > 0xFF);

        Ok(CYCLES_4)
    }

    pub fn sub_a_r(&mut self, source: RegTarget, use_carry: bool) -> Result<CyclesType> {
        let a = self.registers.get_a();
        let value = self.registers.get_register(source)?;
        let carry = if use_carry && self.registers.get_carry() {
            1
        } else {
            0
        };

        let result = a.wrapping_sub(value).wrapping_sub(carry);
        self.registers.set_a(result);

        self.registers.set_zero(result == 0);
        self.registers.set_subtract(true);
        self.registers
            .set_half_carry((a & 0x0F) < (value & 0x0F) + carry);
        self.registers
            .set_carry((a as u16) < (value as u16) + (carry as u16));

        Ok(CYCLES_4)
    }

    pub fn add_a_n(&mut self, use_carry: bool) -> Result<CyclesType> {
        let a = self.registers.get_a();
        let value = self.fetch_byte()?;
        let carry = if use_carry && self.registers.get_carry() {
            1
        } else {
            0
        };

        let result = a.wrapping_add(value).wrapping_add(carry);
        self.registers.set_a(result);

        self.registers.set_zero(result == 0);
        self.registers.set_subtract(false);
        self.registers
            .set_half_carry(((a & 0x0F) + (value & 0x0F) + carry) > 0x0F);
        self.registers
            .set_carry(((a as u16) + (value as u16) + (carry as u16)) > 0xFF);

        Ok(CYCLES_8)
    }

    pub fn sub_a_n(&mut self, use_carry: bool) -> Result<CyclesType> {
        let a = self.registers.get_a();
        let value = self.fetch_byte()?;
        let carry = if use_carry && self.registers.get_carry() {
            1
        } else {
            0
        };

        let result = a.wrapping_sub(value).wrapping_sub(carry);
        self.registers.set_a(result);

        self.registers.set_zero(result == 0);
        self.registers.set_subtract(true);
        self.registers
            .set_half_carry((a & 0x0F) < (value & 0x0F) + carry);
        self.registers
            .set_carry((a as u16) < (value as u16) + (carry as u16));

        Ok(CYCLES_8)
    }

    pub fn increment_pc(&mut self, amount: u16) -> Result<()> {
        let pc = self.registers.get_pc();
        self.registers.set_pc(pc.wrapping_add(amount));
        Ok(())
    }

    pub fn step(&mut self) -> Result<CyclesType> {
        // 保存初始PC
        let start_pc = self.registers.get_pc();

        // 如果在入口點，顯示接下來幾個字節的內容
        if start_pc == 0x0100 {
            if let Ok(mmu) = self.mmu.try_borrow() {
                log::info!("入口點執行狀態：");
                for i in 0..4 {
                    if let Ok(byte) = mmu.read_byte(start_pc + i) {
                        log::info!("  0x{:04X}: 0x{:02X}", start_pc + i, byte);
                    }
                }
            }
        }

        // 讀取指令
        let opcode = self.read_byte(start_pc)?;
        log::debug!("執行指令：PC=0x{:04X}, opcode=0x{:02X}", start_pc, opcode);

        // 執行指令（PC 的更新應該在指令執行過程中進行）
        let cycles = self.decode_and_execute(opcode)?;

        // 檢查PC是否有變化（對於跳轉指令）
        let end_pc = self.registers.get_pc();
        if end_pc == start_pc {
            log::warn!(
                "警告：指令執行後PC未改變：PC=0x{:04X}, opcode=0x{:02X}",
                start_pc,
                opcode
            );
            // 對於非跳轉指令，確保PC指向下一條指令
            self.registers.set_pc(start_pc.wrapping_add(1));
        }

        Ok(cycles)
    }
}

impl ControlInstructions for CPU {
    fn halt(&mut self) -> Result<CyclesType> {
        self.halted = true;
        Ok(CYCLES_4)
    }

    fn stop(&mut self) -> Result<CyclesType> {
        Ok(CYCLES_4)
    }

    fn enable_interrupts(&mut self) -> Result<CyclesType> {
        self.ime_scheduled = true;
        Ok(CYCLES_4)
    }

    fn disable_interrupts(&mut self) -> Result<CyclesType> {
        self.ime = false;
        self.ime_scheduled = false;
        Ok(CYCLES_4)
    }

    fn call(&mut self) -> Result<CyclesType> {
        let addr = self.fetch_word()?;
        let pc = self.registers.get_pc();
        self.push_word(pc)?;
        self.registers.set_pc(addr);
        Ok(CYCLES_24)
    }

    fn call_conditional(&mut self, opcode: u8) -> Result<CyclesType> {
        let condition = match opcode {
            0xC4 => !self.registers.get_zero(),
            0xCC => self.registers.get_zero(),
            0xD4 => !self.registers.get_carry(),
            0xDC => self.registers.get_carry(),
            _ => {
                return Err(Error::Instruction(
                    crate::error::InstructionError::InvalidOpcode(opcode),
                ))
            }
        };

        let addr = self.fetch_word()?;
        if condition {
            let pc = self.registers.get_pc();
            self.push_word(pc)?;
            self.registers.set_pc(addr);
            Ok(CYCLES_24)
        } else {
            Ok(CYCLES_12)
        }
    }

    fn ret(&mut self) -> Result<CyclesType> {
        let addr = self.pop_word()?;
        self.registers.set_pc(addr);
        Ok(CYCLES_16)
    }

    fn ret_conditional(&mut self, opcode: u8) -> Result<CyclesType> {
        let condition = match opcode {
            0xC0 => !self.registers.get_zero(),
            0xC8 => self.registers.get_zero(),
            0xD0 => !self.registers.get_carry(),
            0xD8 => self.registers.get_carry(),
            _ => {
                return Err(Error::Instruction(
                    crate::error::InstructionError::InvalidOpcode(opcode),
                ))
            }
        };

        if condition {
            let addr = self.pop_word()?;
            self.registers.set_pc(addr);
            Ok(CYCLES_20)
        } else {
            Ok(CYCLES_8)
        }
    }

    fn reti(&mut self) -> Result<CyclesType> {
        self.ime = true;
        self.ret()
    }

    fn rst(&mut self, addr: u16) -> Result<CyclesType> {
        let pc = self.registers.get_pc();
        self.push_word(pc)?;
        self.registers.set_pc(addr);
        Ok(CYCLES_16)
    }

    fn pop_bc(&mut self) -> Result<CyclesType> {
        let value = self.pop_word()?;
        self.registers.set_bc(value);
        Ok(CYCLES_12)
    }

    fn pop_de(&mut self) -> Result<CyclesType> {
        let value = self.pop_word()?;
        self.registers.set_de(value);
        Ok(CYCLES_12)
    }

    fn pop_hl(&mut self) -> Result<CyclesType> {
        let value = self.pop_word()?;
        self.registers.set_hl(value);
        Ok(CYCLES_12)
    }

    fn pop_af(&mut self) -> Result<CyclesType> {
        let value = self.pop_word()?;
        self.registers.set_af(value);
        Ok(CYCLES_12)
    }

    fn push_bc(&mut self) -> Result<CyclesType> {
        self.push_word(self.registers.get_bc())?;
        Ok(CYCLES_16)
    }

    fn push_de(&mut self) -> Result<CyclesType> {
        self.push_word(self.registers.get_de())?;
        Ok(CYCLES_16)
    }

    fn push_hl(&mut self) -> Result<CyclesType> {
        self.push_word(self.registers.get_hl())?;
        Ok(CYCLES_16)
    }

    fn push_af(&mut self) -> Result<CyclesType> {
        let af = ((self.registers.get_a() as u16) << 8) | (self.registers.get_f() as u16);
        self.push_word(af)?;
        Ok(CYCLES_16)
    }

    fn jump(&mut self) -> Result<CyclesType> {
        let current_pc = self.registers.get_pc();
        let addr = self.fetch_word()?;
        log::info!(
            "執行跳轉指令：從 0x{:04X} 跳轉到 0x{:04X}",
            current_pc.wrapping_sub(3),
            addr
        );

        // 檢查跳轉地址的合法性
        if addr >= 0x8000 {
            log::warn!("跳轉到非法地址：0x{:04X}", addr);
            return Err(Error::Hardware(HardwareError::MemoryMap(format!(
                "非法跳轉地址：0x{:04X}",
                addr
            ))));
        }

        self.registers.set_pc(addr);
        Ok(CYCLES_16)
    }
}
