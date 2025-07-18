// decode_and_execute.rs - CPU指令解碼與執行
// 2025.07.18

use super::cycles::*;
use super::registers::Registers;
use super::instructions::*;
use crate::core::error::{Error, InstructionError, Result};
use crate::core::mmu::MMU;
use log::{debug, error, warn};

// 非法操作碼列表
const ILLEGAL_OPCODES: [u8; 11] = [0xD3, 0xDB, 0xDD, 0xE3, 0xE4, 0xEB, 0xEC, 0xED, 0xF4, 0xFC, 0xFD];

impl CPU {
    pub fn decode_and_execute(&mut self, opcode: u8) -> Result<CyclesType> {
        // 檢查是否為非法操作碼
        if ILLEGAL_OPCODES.contains(&opcode) {
            error!("非法操作碼: 0x{:02X} 在 PC=0x{:04X}", opcode, self.registers.get_pc());
            return Err(Error::Instruction(InstructionError::InvalidOpcode(opcode)));
        }

        // 記錄指令執行資訊
        debug!(
            "執行指令 0x{:02X} 在 PC=0x{:04X}, A=0x{:02X}, F=0x{:02X}, BC=0x{:04X}, DE=0x{:04X}, HL=0x{:04X}, SP=0x{:04X}",
            opcode,
            self.registers.get_pc(),
            self.registers.get_a(),
            self.registers.get_f(),
            self.registers.get_bc(),
            self.registers.get_de(),
            self.registers.get_hl(),
            self.registers.get_sp()
        );

        // HALT狀態處理
        if self.halted {
            self.check_and_handle_interrupts()?;
            return Ok(CYCLES_HALT);
        }

        // 指令分派和執行
        let cycles = match opcode {
            // 控制指令
            0x00 => control::dispatch(self, opcode)?,     // NOP
            0x76 => control::dispatch(self, opcode)?,     // HALT
            0xF3 | 0xFB => control::dispatch(self, opcode)?, // DI/EI

            // 載入指令
            0x01..=0x3E => load::dispatch(self, opcode)?,  // LD系列指令

            // 算術運算指令
            0x04..=0x3D => arithmetic::dispatch(self, opcode)?, // INC/DEC
            0x80..=0x9F => arithmetic::dispatch(self, opcode)?, // ADD/ADC/SUB/SBC
            0xC6 | 0xCE | 0xD6 | 0xDE => arithmetic::dispatch(self, opcode)?, // 立即數運算

            // 邏輯運算指令
            0xA0..=0xBF => logic::dispatch(self, opcode)?, // AND/OR/XOR/CP
            0xE6 | 0xEE | 0xF6 | 0xFE => logic::dispatch(self, opcode)?, // 立即數邏輯運算

            // 跳轉和呼叫指令
            0xC3 | 0xC2 | 0xCA | 0xD2 | 0xDA => jump::dispatch(self, opcode)?, // JP
            0x18 | 0x20 | 0x28 | 0x30 | 0x38 => jump::dispatch(self, opcode)?, // JR
            0xCD | 0xC4 | 0xCC | 0xD4 | 0xDC => call::dispatch(self, opcode)?, // CALL
            0xC9 | 0xC0 | 0xC8 | 0xD0 | 0xD8 | 0xD9 => control::dispatch(self, opcode)?, // RET/RETI

            // CB前綴指令
            0xCB => {
                let cb_opcode = self.fetch_byte()?;
                bit::dispatch(self, cb_opcode)?
            }

            // RST指令
            0xC7 | 0xCF | 0xD7 | 0xDF | 0xE7 | 0xEF | 0xF7 | 0xFF => {
                let addr = match opcode {
                    0xC7 => 0x00, 0xCF => 0x08, 0xD7 => 0x10, 0xDF => 0x18,
                    0xE7 => 0x20, 0xEF => 0x28, 0xF7 => 0x30, 0xFF => 0x38,
                    _ => unreachable!()
                };
                self.rst(addr)?
            }

            _ => {
                warn!("未實現的指令: 0x{:02X} 在 PC=0x{:04X}", opcode, self.registers.get_pc());
                return Err(Error::Instruction(InstructionError::UnimplementedOpcode(opcode)));
            }
        };

        // 執行後處理
        self.instruction_count += 1;
        self.check_and_handle_interrupts()?;
        
        Ok(cycles)
    }

    /// 處理中斷
    fn check_and_handle_interrupts(&mut self) -> Result<()> {
        if !self.ime {
            return Ok(());
        }

        // 檢查並處理中斷...
        let interrupt_enable = self.read_byte(0xFFFF)?;
        let interrupt_flags = self.read_byte(0xFF0F)?;
        let pending_interrupts = interrupt_enable & interrupt_flags;

        if pending_interrupts == 0 {
            return Ok(());
        }

        // 處理具體中斷邏輯...
        self.halted = false;
        
        Ok(())
    }

    /// 處理RST指令
    fn rst(&mut self, addr: u8) -> Result<CyclesType> {
        let pc = self.registers.get_pc();
        self.push_word(pc)?;
        self.registers.set_pc(addr as u16);
        Ok(CYCLES_4)
    }
}
