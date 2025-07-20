use crate::core::cpu::cpu::CPU;
use crate::core::cpu::register_utils::FlagOperations;
use crate::core::cycles::*;

pub fn dispatch(cpu: &mut CPU, opcode: u8) -> Result<CyclesType, crate::core::error::Error> {
    match opcode {
        0x76 => Ok(cpu.halt()),
        0x10 => Ok(cpu.stop()),
        0xFB => Ok(cpu.enable_interrupts()),
        0xF3 => Ok(cpu.disable_interrupts()),
        0xCD => Ok(CPU::call(cpu, 0)),
        0xC3 => cpu.jump(),
        0xC4 | 0xCC | 0xD4 | 0xDC => {
            let condition = (opcode >> 3) & 0x03;
            Ok(cpu.call_conditional(condition))
        }
        0xC9 => Ok(CPU::ret(cpu)),
        0xC0 | 0xC8 | 0xD0 | 0xD8 => {
            let condition = (opcode >> 3) & 0x03;
            Ok(cpu.ret_conditional(condition))
        }
        0xD9 => Ok(CPU::ret(cpu)),
        0xC7 | 0xCF | 0xD7 | 0xDF | 0xE7 | 0xEF | 0xF7 | 0xFF => {
            let address = (opcode & 0x38) as u16;
            Ok(CPU::rst(cpu, 0))
        }
        0x00 => Ok(CYCLES_1),
        0xC1 => cpu.pop_bc(),
        0xD1 => cpu.pop_de(),
        0xE1 => cpu.pop_hl(),
        0xF1 => cpu.pop_af(),
        0xC5 => cpu.push_bc(),
        0xD5 => cpu.push_de(),
        0xE5 => cpu.push_hl(),
        0xF5 => cpu.push_af(),
        _ => Err(crate::core::error::Error::Instruction(
            crate::core::error::InstructionError::InvalidOpcode(opcode),
        )),
    }
}

impl CPU {
    // NOP 指令 (00)
    pub fn nop(&mut self) -> CyclesType {
        // TODO: 實作 NOP
        4
    }

    // STOP 指令 (10)
    pub fn stop(&mut self) -> CyclesType {
        // TODO: 實作 STOP
        4
    }

    // HALT 指令 (76)
    pub fn halt(&mut self) -> CyclesType {
        // TODO: 實作 HALT
        4
    }

    // DI 指令 (F3)
    pub fn di(&mut self) -> CyclesType {
        // TODO: 實作 DI
        4
    }

    // EI 指令 (FB)
    pub fn ei(&mut self) -> CyclesType {
        // TODO: 實作 EI
        4
    }
}
