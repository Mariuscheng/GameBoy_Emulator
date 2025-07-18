#![allow(unused_variables)]
#![allow(dead_code)]

use super::prelude::*;
use std::io::Write;

pub fn dispatch(cpu: &mut CPU, opcode: u8) -> Result<CyclesType> {
    match opcode {
        // JP nn - 絕對跳轉
        0xC3 => {
            let address = cpu.fetch_word()?;
            cpu.log_instruction("JP nn", &format!("跳轉到 0x{:04X}", address));
            cpu.registers.set_pc(address);
            Ok(CYCLES_3)
        }

        // JP cc,nn - 條件跳轉
        0xC2 | 0xCA | 0xD2 | 0xDA => {
            let address = cpu.fetch_word()?;
            let condition = match opcode {
                0xC2 => (!cpu.registers.get_zero(), "NZ"),
                0xCA => (cpu.registers.get_zero(), "Z"),
                0xD2 => (!cpu.registers.get_carry(), "NC"),
                0xDA => (cpu.registers.get_carry(), "C"),
                _ => unreachable!(),
            };

            cpu.log_instruction(
                &format!("JP {}", condition.1),
                &format!("目標地址 0x{:04X}", address),
            );

            if condition.0 {
                cpu.registers.set_pc(address);
                Ok(CYCLES_3)
            } else {
                Ok(CYCLES_2)
            }
        }

        // JR e - 相對跳轉
        0x18 => {
            let offset = cpu.fetch_byte()? as i8;
            let pc = cpu.registers.get_pc();
            let target = ((pc as i32) + (offset as i32)) as u16;
            cpu.log_instruction("JR", &format!("跳轉到 0x{:04X}", target));
            cpu.registers.set_pc(target);
            Ok(CYCLES_2)
        }

        // JR cc,e - 條件相對跳轉
        0x20 | 0x28 | 0x30 | 0x38 => {
            let offset = cpu.fetch_byte()? as i8;
            let condition = match opcode {
                0x20 => (!cpu.registers.get_zero(), "NZ"),
                0x28 => (cpu.registers.get_zero(), "Z"),
                0x30 => (!cpu.registers.get_carry(), "NC"),
                0x38 => (cpu.registers.get_carry(), "C"),
                _ => unreachable!(),
            };

            let pc = cpu.registers.get_pc();
            let target = ((pc as i32) + (offset as i32)) as u16;
            cpu.log_instruction(
                &format!("JR {}", condition.1),
                &format!("目標地址 0x{:04X}", target),
            );

            if condition.0 {
                cpu.registers.set_pc(target);
                Ok(CYCLES_2)
            } else {
                Ok(CYCLES_1)
            }
        }

        // JP (HL) - 跳轉到HL指向的地址
        0xE9 => {
            let target = cpu.registers.get_hl();
            cpu.log_instruction("JP (HL)", &format!("跳轉到 0x{:04X}", target));
            cpu.registers.set_pc(target);
            Ok(CYCLES_1)
        }

        _ => Err(Error::Instruction(InstructionError::InvalidOpcode(opcode))),
    }
}

impl CPU {
    fn log_instruction(&mut self, instruction_name: &str, details: &str) {
        if let Ok(mut file) = std::fs::OpenOptions::new()
            .create(true)
            .append(true)
            .open("logs/cpu_exec.log")
        {
            writeln!(
                file,
                "PC={:04X} | {} | {} | AF={:04X} BC={:04X} DE={:04X} HL={:04X}",
                self.registers.get_pc(),
                instruction_name,
                details,
                self.registers.get_af(),
                self.registers.get_bc(),
                self.registers.get_de(),
                self.registers.get_hl()
            )
            .ok();
        }
    }

    pub fn jp_nn(&mut self) -> Result<CyclesType> {
        let address = self.fetch_word()?;
        self.log_instruction("JP nn", &format!("跳轉到 0x{:04X}", address));
        self.registers.set_pc(address);
        Ok(CYCLES_3)
    }
    pub fn jp_cc_nn(&mut self, condition: u8) -> Result<CyclesType> {
        let address = self.fetch_word()?;
        let condition_name = match condition {
            0 => "NZ",
            1 => "Z",
            2 => "NC",
            3 => "C",
            _ => {
                return Err(Error::Instruction(InstructionError::InvalidCondition(
                    condition,
                )));
            }
        };

        let jump = match condition {
            0 => !self.registers.get_zero(),  // NZ
            1 => self.registers.get_zero(),   // Z
            2 => !self.registers.get_carry(), // NC
            3 => self.registers.get_carry(),  // C
            _ => unreachable!(),
        };

        self.log_instruction(
            &format!("JP {}, nn", condition_name),
            &format!(
                "目標=0x{:04X} ({}執行)",
                address,
                if jump { "已" } else { "未" }
            ),
        );

        if jump {
            self.registers.set_pc(address);
            Ok(CYCLES_3)
        } else {
            Ok(CYCLES_2)
        }
    }

    pub fn jp_hl(&mut self) -> Result<CyclesType> {
        let target = self.registers.get_hl();
        self.log_instruction("JP (HL)", &format!("跳轉到 (HL) = 0x{:04X}", target));
        self.registers.set_pc(target);
        Ok(CYCLES_1)
    }

    pub fn jr_n(&mut self) -> Result<CyclesType> {
        let offset = self.fetch_byte()? as i8;
        let current_pc = self.registers.get_pc();
        let target = ((current_pc as i32) + (offset as i32)) as u16;
        self.log_instruction(
            "JR n",
            &format!("跳轉到 PC + {:+} = 0x{:04X}", offset, target),
        );
        self.registers.set_pc(target);
        Ok(CYCLES_2)
    }

    pub fn jr_cc_n(&mut self, condition: u8) -> Result<CyclesType> {
        let offset = self.fetch_byte()? as i8;
        let current_pc = self.registers.get_pc();
        let target = ((current_pc as i32) + (offset as i32)) as u16;
        let condition_name = match condition {
            0 => "NZ",
            1 => "Z",
            2 => "NC",
            3 => "C",
            _ => {
                return Err(Error::Instruction(InstructionError::Custom(
                    "無效的條件碼".to_string(),
                )));
            }
        };

        let jump = match condition {
            0 => !self.registers.get_zero(),  // NZ
            1 => self.registers.get_zero(),   // Z
            2 => !self.registers.get_carry(), // NC
            3 => self.registers.get_carry(),  // C
            _ => unreachable!(),
        };

        self.log_instruction(
            &format!("JR {}, n", condition_name),
            &format!(
                "目標=0x{:04X} ({}執行)",
                target,
                if jump { "已" } else { "未" }
            ),
        );

        if jump {
            self.registers.set_pc(target);
            Ok(CYCLES_2)
        } else {
            Ok(CYCLES_2)
        }
    }

    pub fn call_nn(&mut self) -> Result<CyclesType> {
        let return_addr = self.registers.get_pc();
        let address = self.fetch_word()?;
        self.push_word(return_addr)?;
        self.log_instruction("CALL nn", &format!("跳轉到 0x{:04X}", address));
        self.registers.set_pc(address);
        Ok(CYCLES_3)
    }

    pub fn call_cc_nn(&mut self, condition: u8) -> Result<CyclesType> {
        let address = self.fetch_word()?;
        let condition_name = match condition {
            0 => "NZ",
            1 => "Z",
            2 => "NC",
            3 => "C",
            _ => {
                return Err(Error::Instruction(InstructionError::Custom(
                    "無效的條件碼".to_string(),
                )));
            }
        };

        let jump = match condition {
            0 => !self.registers.get_zero(),  // NZ
            1 => self.registers.get_zero(),   // Z
            2 => !self.registers.get_carry(), // NC
            3 => self.registers.get_carry(),  // C
            _ => unreachable!(),
        };

        self.log_instruction(
            &format!("CALL {}, nn", condition_name),
            &format!(
                "目標=0x{:04X} ({}執行)",
                address,
                if jump { "已" } else { "未" }
            ),
        );

        if jump {
            let return_addr = self.registers.get_pc();
            self.push_word(return_addr)?;
            self.registers.set_pc(address);
            Ok(CYCLES_3)
        } else {
            Ok(CYCLES_2)
        }
    }

    pub fn return_no_condition(&mut self) -> Result<CyclesType> {
        let return_addr = self.pop_word()?;
        self.log_instruction("RET", &format!("返回到 0x{:04X}", return_addr));
        self.registers.set_pc(return_addr);
        Ok(CYCLES_2)
    }

    pub fn return_if_condition(&mut self, condition: u8) -> Result<CyclesType> {
        let condition_name = match condition {
            0 => "NZ",
            1 => "Z",
            2 => "NC",
            3 => "C",
            _ => {
                return Err(Error::Instruction(InstructionError::Custom(
                    "無效的條件碼".to_string(),
                )));
            }
        };

        let jump = match condition {
            0 => !self.registers.get_zero(),  // NZ
            1 => self.registers.get_zero(),   // Z
            2 => !self.registers.get_carry(), // NC
            3 => self.registers.get_carry(),  // C
            _ => unreachable!(),
        };

        if jump {
            let return_addr = self.pop_word()?;
            self.log_instruction(
                &format!("RET {}", condition_name),
                &format!("返回到 0x{:04X}", return_addr),
            );
            log::debug!(
                "[RET] PC 條件返回到 0x{:04X} (條件: {})",
                return_addr,
                condition_name
            );
            self.registers.set_pc(return_addr);
            Ok(CYCLES_3)
        } else {
            Ok(CYCLES_1)
        }
    }

    pub fn return_and_enable_interrupts(&mut self) -> Result<CyclesType> {
        let return_addr = self.pop_word()?;
        self.log_instruction("RETI", &format!("返回到 0x{:04X}", return_addr));
        self.registers.set_pc(return_addr);
        self.ime = true;
        Ok(CYCLES_2)
    }
}
