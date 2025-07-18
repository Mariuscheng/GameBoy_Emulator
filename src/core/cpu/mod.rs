pub mod flags;
pub mod instructions;
pub mod register_target;
pub mod registers;

use self::instructions::control::ControlInstructions;
use self::instructions::register_utils::FlagOperations;
use self::register_target::RegTarget;
use self::registers::Registers;
use crate::core::cycles::*;
use crate::core::error::{Error, HardwareError, InstructionError, Result};
use crate::core::mmu::MMU;
use log::{debug, error, warn};
use std::{cell::RefCell, rc::Rc};

// 定義週期常量
const CYCLES_4: CyclesType = 4;
const CYCLES_8: CyclesType = 8;
const CYCLES_12: CyclesType = 12;
const CYCLES_16: CyclesType = 16;
// const CYCLES_20: CyclesType = 20;
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
    pub fn reset(&mut self) -> Result<()> {
        self.registers.set_pc(0x0100);
        self.registers.set_af(0x01B0);
        self.registers.set_bc(0x0013);
        self.registers.set_de(0x00D8);
        self.registers.set_hl(0x014D);
        self.registers.set_sp(0xFFFE);
        self.halted = false;
        self.ime = false;
        self.ime_scheduled = false;
        self.instruction_count = 0;
        Ok(())
    }

    pub fn get_pc(&self) -> u16 {
        self.registers.get_pc()
    }

    pub fn set_pc(&mut self, value: u16) {
        self.registers.set_pc(value)
    }

    pub fn read_byte(&mut self, addr: u16) -> Result<u8> {
        if let Ok(mmu) = self.mmu.try_borrow() {
            mmu.read_byte(addr)
        } else {
            Err(Error::Hardware(HardwareError::BorrowError))
        }
    }

    pub fn write_byte(&mut self, addr: u16, value: u8) -> Result<()> {
        if let Ok(mut mmu) = self.mmu.try_borrow_mut() {
            mmu.write_byte(addr, value)
        } else {
            Err(Error::Hardware(HardwareError::BorrowError))
        }
    }

    pub fn fetch_byte(&mut self) -> Result<u8> {
        let pc = self.registers.get_pc();
        let byte = self.read_byte(pc)?;
        log::debug!("讀取位元組 0x{:02X} 從 PC=0x{:04X}", byte, pc);
        self.registers.set_pc(pc.wrapping_add(1));
        Ok(byte)
    }

    pub fn fetch_word(&mut self) -> Result<u16> {
        let original_pc = self.registers.get_pc();

        // 判斷當前位置是否為JP指令
        let is_jump_instruction = original_pc == 0x0101 || original_pc == 0x0150;

        // 如果是JP指令，先跳過指令字節
        if is_jump_instruction {
            let op = self.read_byte(original_pc)?;
            if op != 0xC3 {
                log::warn!(
                    "預期是JP指令(0xC3)，但在PC=0x{:04X}找到0x{:02X}",
                    original_pc,
                    op
                );
            }
            self.registers.set_pc(original_pc + 1);
        }

        // 讀取兩個字節（地址或值）
        let addr_low = self.fetch_byte()?;
        let addr_high = self.fetch_byte()?;
        let word = ((addr_high as u16) << 8) | (addr_low as u16);

        // 記錄日誌
        if is_jump_instruction {
            log::debug!(
                "讀取跳轉目標: 0x{:04X} (從 PC=0x{:04X}, 低字節=0x{:02X}, 高字節=0x{:02X})",
                word,
                original_pc,
                addr_low,
                addr_high
            );
        } else {
            log::debug!(
                "讀取16位值: 0x{:04X} (從 PC=0x{:04X}, 低字節=0x{:02X}, 高字節=0x{:02X})",
                word,
                original_pc,
                addr_low,
                addr_high
            );
        }

        Ok(word)
    }
    pub fn push_word(&mut self, value: u16) -> Result<()> {
        let sp = self.registers.get_sp();
        self.write_byte(sp.wrapping_sub(1), ((value >> 8) & 0xFF) as u8)?;
        self.write_byte(sp.wrapping_sub(2), (value & 0xFF) as u8)?;
        self.registers.set_sp(sp.wrapping_sub(2));
        Ok(())
    }

    pub fn pop_word(&mut self) -> Result<u16> {
        let sp = self.registers.get_sp();
        let low = self.read_byte(sp)?;
        let high = self.read_byte(sp.wrapping_add(1))?;
        let value = ((high as u16) << 8) | (low as u16);
        self.registers.set_sp(sp.wrapping_add(2));
        Ok(value)
    }
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
        // 更新指令計數器
        self.instruction_count += 1;

        // 檢查HALT狀態
        if self.halted {
            self.check_and_handle_interrupts()?;
            return Ok(CYCLES_4);
        }

        let current_pc = self.registers.get_pc();

        // 紀錄詳細的CPU狀態
        debug!(
            "CPU狀態: PC=0x{:04X}, SP=0x{:04X}, A=0x{:02X}, F=0x{:02X}, BC=0x{:04X}, DE=0x{:04X}, HL=0x{:04X}",
            current_pc,
            self.registers.get_sp(),
            self.registers.get_a(),
            self.registers.get_f(),
            self.registers.get_bc(),
            self.registers.get_de(),
            self.registers.get_hl()
        );

        // 檢查非法操作碼
        if [
            0xD3, 0xDB, 0xDD, 0xE3, 0xE4, 0xEB, 0xEC, 0xED, 0xF4, 0xFC, 0xFD,
        ]
        .contains(&opcode)
        {
            error!("非法操作碼: 0x{:02X} 在 PC=0x{:04X}", opcode, current_pc);
            return Err(Error::Instruction(InstructionError::InvalidOpcode(opcode)));
        }

        debug!("執行指令 0x{:02X} 在 PC=0x{:04X}", opcode, current_pc);

        let result = match opcode {
            // 基本指令
            0x00 => Ok(CYCLES_4), // NOP

            // 載入指令
            0x31 => self.ld_sp_nn(), // LD SP, nn
            0x01..=0x3E => instructions::load::dispatch(self, opcode),

            // RST指令
            0xC7 => self.rst(0x00), // RST 00H
            0xCF => self.rst(0x08), // RST 08H
            0xD7 => self.rst(0x10), // RST 10H
            0xDF => self.rst(0x18), // RST 18H
            0xE7 => self.rst(0x20), // RST 20H
            0xEF => self.rst(0x28), // RST 28H
            0xF7 => self.rst(0x30), // RST 30H
            0xFF => self.rst(0x38), // RST 38H

            // CB前綴指令
            0xCB => {
                let cb_opcode = self.fetch_byte()?;
                instructions::bit::dispatch(self, cb_opcode)
            }
            0xF7 => self.rst(0x30), // RST 30H
            0xCD => self.call(),    // CALL nn
            0xC4 => self.call_nz(), // CALL NZ,nn
            0xCC => self.call_z(),  // CALL Z,nn
            0xD4 => self.call_nc(), // CALL NC,nn
            0xDC => self.call_c(),  // CALL C,nn
            _ => instructions::execute(self, opcode),
        };

        // 在指令執行後處理中斷
        if let Ok(cycles) = result {
            self.check_and_handle_interrupts()?;
            Ok(cycles)
        } else {
            result
        }
    }
}

impl ControlInstructions for CPU {
    fn halt(&mut self) -> Result<CyclesType> {
        self.halted = true;
        Ok(CYCLES_4)
    }

    fn stop(&mut self) -> Result<CyclesType> {
        self.halted = true;
        Ok(CYCLES_4)
    }

    fn enable_interrupts(&mut self) -> Result<CyclesType> {
        self.ime_scheduled = true;
        self.increment_pc(1)?;
        Ok(CYCLES_4)
    }

    fn disable_interrupts(&mut self) -> Result<CyclesType> {
        self.ime = false;
        self.increment_pc(1)?;
        Ok(CYCLES_4)
    }

    fn jump(&mut self) -> Result<CyclesType> {
        let current_pc = self.registers.get_pc();
        if current_pc != 0x0101 && current_pc != 0x0150 {
            log::warn!("在非預期位置執行跳轉指令: PC=0x{:04X}", current_pc);
            return Ok(CYCLES_4);
        }
        let jump_target = self.fetch_word()?;
        log::info!(
            "執行跳轉: 從 PC=0x{:04X} 跳轉到 0x{:04X}",
            current_pc,
            jump_target
        );
        self.registers.set_pc(jump_target);
        Ok(CYCLES_12)
    }

    fn call(&mut self) -> Result<CyclesType> {
        let current_pc = self.registers.get_pc();
        let target = self.fetch_word()?;
        log::debug!(
            "執行CALL指令: 從PC=0x{:04X}調用地址0x{:04X}",
            current_pc,
            target
        );

        // 保存下一條指令的地址
        let return_addr = current_pc.wrapping_add(2);
        self.push_word(return_addr)?;

        // 跳轉到目標地址
        self.registers.set_pc(target);
        Ok(CYCLES_24)
    }

    fn ret(&mut self) -> Result<CyclesType> {
        let addr = self.pop_word()?;
        log::debug!("執行RET指令: 返回到地址0x{:04X}", addr);
        self.registers.set_pc(addr);
        Ok(CYCLES_16)
    }

    fn reti(&mut self) -> Result<CyclesType> {
        self.ime = true;
        self.ret()
    }

    fn rst(&mut self, addr: u16) -> Result<CyclesType> {
        let current_pc = self.registers.get_pc();
        log::debug!("執行RST 0x{:02X}: 從PC=0x{:04X}跳轉", addr, current_pc);

        // 保存當前PC到堆疊
        let next_pc = current_pc.wrapping_add(1);
        self.push_word(next_pc)?;

        // 跳轉到RST目標地址
        self.registers.set_pc(addr);
        Ok(CYCLES_16)
    }

    fn call_conditional(&mut self, condition: u8) -> Result<CyclesType> {
        let current_pc = self.registers.get_pc();
        let target = self.fetch_word()?;
        let should_call = match condition {
            0 => !self.registers.get_zero(),  // NZ
            1 => self.registers.get_zero(),   // Z
            2 => !self.registers.get_carry(), // NC
            3 => self.registers.get_carry(),  // C
            _ => {
                return Err(Error::Instruction(InstructionError::InvalidCondition(
                    condition,
                )));
            }
        };

        if should_call {
            log::debug!(
                "條件CALL 成功: 從PC=0x{:04X}調用地址0x{:04X}",
                current_pc,
                target
            );
            let return_addr = current_pc.wrapping_add(2);
            self.push_word(return_addr)?;
            self.registers.set_pc(target);
            Ok(CYCLES_24)
        } else {
            log::debug!("條件CALL 失敗: 在PC=0x{:04X}繼續執行", current_pc);
            self.registers.set_pc(current_pc.wrapping_add(3));
            Ok(CYCLES_12)
        }
    }

    fn ret_conditional(&mut self, condition: u8) -> Result<CyclesType> {
        let should_return = match condition {
            0 => !self.registers.get_zero(),  // NZ
            1 => self.registers.get_zero(),   // Z
            2 => !self.registers.get_carry(), // NC
            3 => self.registers.get_carry(),  // C
            _ => {
                return Err(Error::Instruction(InstructionError::InvalidCondition(
                    condition,
                )));
            }
        };

        if should_return {
            self.ret()
        } else {
            Ok(CYCLES_8)
        }
    }

    fn push_af(&mut self) -> Result<CyclesType> {
        let value = self.registers.get_af();
        self.push_word(value)?;
        Ok(CYCLES_16)
    }

    fn push_bc(&mut self) -> Result<CyclesType> {
        let value = self.registers.get_bc();
        self.push_word(value)?;
        Ok(CYCLES_16)
    }

    fn push_de(&mut self) -> Result<CyclesType> {
        let value = self.registers.get_de();
        self.push_word(value)?;
        Ok(CYCLES_16)
    }

    fn push_hl(&mut self) -> Result<CyclesType> {
        let value = self.registers.get_hl();
        self.push_word(value)?;
        Ok(CYCLES_16)
    }

    fn pop_af(&mut self) -> Result<CyclesType> {
        let value = self.pop_word()?;
        self.registers.set_af(value);
        Ok(CYCLES_12)
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
}

impl CPU {
    pub fn increment_pc(&mut self, amount: u16) -> Result<()> {
        let current_pc = self.registers.get_pc();
        self.registers.set_pc(current_pc.wrapping_add(amount));
        Ok(())
    }

    pub fn step(&mut self) -> Result<CyclesType> {
        let opcode = self.fetch_byte()?;
        self.decode_and_execute(opcode)
    }

    pub fn call_nz(&mut self) -> Result<CyclesType> {
        self.call_conditional(0)
    }

    pub fn call_z(&mut self) -> Result<CyclesType> {
        self.call_conditional(1)
    }

    pub fn call_nc(&mut self) -> Result<CyclesType> {
        self.call_conditional(2)
    }

    pub fn call_c(&mut self) -> Result<CyclesType> {
        self.call_conditional(3)
    }

    pub fn jr_nz(&mut self) -> Result<CyclesType> {
        if !self.registers.get_zero() {
            let offset = self.fetch_byte()? as i8;
            let current_pc = self.registers.get_pc();
            let new_pc = ((current_pc as i32) + (offset as i32)) as u16;
            self.registers.set_pc(new_pc);
            Ok(CYCLES_12)
        } else {
            self.increment_pc(1)?;
            Ok(CYCLES_8)
        }
    }

    pub fn ld_a_n(&mut self) -> Result<CyclesType> {
        let value = self.fetch_byte()?;
        self.registers.set_a(value);
        Ok(CYCLES_8)
    }

    pub fn add_hl_de(&mut self) -> Result<CyclesType> {
        let hl = self.registers.get_hl();
        let de = self.registers.get_de();
        let (result, carry) = hl.overflowing_add(de);

        // 設置標誌位
        self.registers.set_subtract(false);
        self.registers.set_carry(carry);
        self.registers
            .set_half_carry(((hl & 0xFFF) + (de & 0xFFF)) > 0xFFF);

        self.registers.set_hl(result);
        Ok(CYCLES_8)
    }

    pub fn check_and_handle_interrupts(&mut self) -> Result<()> {
        if !self.ime {
            return Ok(());
        }

        // 讀取中斷標誌和啟用寄存器
        let if_flags = self.read_byte(0xFF0F)?;
        let ie_flags = self.read_byte(0xFFFF)?;
        let pending = if_flags & ie_flags & 0x1F;

        if pending == 0 {
            return Ok(());
        }

        // 處理中斷
        self.halted = false;
        self.ime = false;

        for i in 0..5 {
            if (pending & (1 << i)) != 0 {
                // 清除中斷標誌
                let new_if = if_flags & !(1 << i);
                self.write_byte(0xFF0F, new_if)?;

                // 保存當前PC並跳轉到中斷處理程序
                let pc = self.registers.get_pc();
                self.push_word(pc)?;

                // 設置新的PC
                let vector = 0x0040 + (i * 0x08);
                self.registers.set_pc(vector as u16);
                break;
            }
        }

        Ok(())
    }
}

impl CPU {
    // 寄存器存取方法
    pub fn get_a(&self) -> u8 {
        (self.registers.get_af() >> 8) as u8
    }

    pub fn set_a(&mut self, value: u8) {
        let af = (self.registers.get_af() & 0x00FF) | ((value as u16) << 8);
        self.registers.set_af(af);
    }

    pub fn get_b(&self) -> u8 {
        (self.registers.get_bc() >> 8) as u8
    }

    pub fn set_b(&mut self, value: u8) {
        let bc = (self.registers.get_bc() & 0x00FF) | ((value as u16) << 8);
        self.registers.set_bc(bc);
    }

    // 標誌位操作，使用 FlagOperations trait
    pub fn get_flag_z(&self) -> bool {
        self.registers.get_zero()
    }

    pub fn set_flag_z(&mut self, value: bool) {
        self.registers.set_zero(value)
    }

    pub fn get_flag_n(&self) -> bool {
        self.registers.get_subtract()
    }

    pub fn set_flag_n(&mut self, value: bool) {
        self.registers.set_subtract(value)
    }

    pub fn get_flag_h(&self) -> bool {
        self.registers.get_half_carry()
    }

    pub fn set_flag_h(&mut self, value: bool) {
        self.registers.set_half_carry(value)
    }

    pub fn get_flag_c(&self) -> bool {
        self.registers.get_carry()
    }

    pub fn set_flag_c(&mut self, value: bool) {
        self.registers.set_carry(value)
    }

    // INC指令實現
    pub fn inc_r(&mut self, target: RegTarget) -> Result<CyclesType> {
        let value = match target {
            RegTarget::A => self.get_a(),
            RegTarget::B => self.get_b(),
            RegTarget::C => (self.registers.get_bc() & 0xFF) as u8,
            RegTarget::D => (self.registers.get_de() >> 8) as u8,
            RegTarget::E => (self.registers.get_de() & 0xFF) as u8,
            RegTarget::H => (self.registers.get_hl() >> 8) as u8,
            RegTarget::L => (self.registers.get_hl() & 0xFF) as u8,
            RegTarget::MemHL => {
                let addr = self.registers.get_hl();
                self.read_byte(addr)?
            }
            other => return Err(Error::Instruction(InstructionError::InvalidRegister(other))),
        };

        let result = value.wrapping_add(1);
        let half_carry = (value & 0x0F) == 0x0F;

        match target {
            RegTarget::A => self.set_a(result),
            RegTarget::B => self.set_b(result),
            RegTarget::C => {
                let bc = (self.registers.get_bc() & 0xFF00) | result as u16;
                self.registers.set_bc(bc);
            }
            RegTarget::D => {
                let de = (self.registers.get_de() & 0x00FF) | ((result as u16) << 8);
                self.registers.set_de(de);
            }
            RegTarget::E => {
                let de = (self.registers.get_de() & 0xFF00) | result as u16;
                self.registers.set_de(de);
            }
            RegTarget::H => {
                let hl = (self.registers.get_hl() & 0x00FF) | ((result as u16) << 8);
                self.registers.set_hl(hl);
            }
            RegTarget::L => {
                let hl = (self.registers.get_hl() & 0xFF00) | result as u16;
                self.registers.set_hl(hl);
            }
            RegTarget::MemHL => {
                let addr = self.registers.get_hl();
                self.write_byte(addr, result)?;
            }
            other => return Err(Error::Instruction(InstructionError::InvalidRegister(other))),
        }

        self.set_flag_z(result == 0);
        self.set_flag_n(false);
        self.set_flag_h(half_carry);

        Ok(match target {
            RegTarget::MemHL => 12, // MemHL需要12個週期
            _ => 4,                 // 寄存器操作需要4個週期
        })
    }

    // 條件返回指令
    pub fn ret_z(&mut self) -> Result<CyclesType> {
        if self.get_flag_z() {
            let low = self.read_byte(self.registers.get_sp())?;
            let high = self.read_byte(self.registers.get_sp().wrapping_add(1))?;
            let addr = ((high as u16) << 8) | (low as u16);

            self.registers
                .set_sp(self.registers.get_sp().wrapping_add(2));
            self.registers.set_pc(addr);

            Ok(20) // 如果條件為真，需要20個週期
        } else {
            self.registers
                .set_pc(self.registers.get_pc().wrapping_add(1));
            Ok(8) // 如果條件為假，需要8個週期
        }
    }

    // 十進制調整
    pub fn daa(&mut self) -> Result<CyclesType> {
        let mut a = self.get_a();
        let mut adjust = 0;

        if self.get_flag_h() || (!self.get_flag_n() && (a & 0x0F) > 9) {
            adjust |= 0x06;
        }

        if self.get_flag_c() || (!self.get_flag_n() && a > 0x99) {
            adjust |= 0x60;
            self.set_flag_c(true);
        }

        a = if self.get_flag_n() {
            a.wrapping_sub(adjust)
        } else {
            a.wrapping_add(adjust)
        };

        self.set_flag_z(a == 0);
        self.set_flag_h(false);
        self.set_a(a);

        Ok(4)
    }
}
