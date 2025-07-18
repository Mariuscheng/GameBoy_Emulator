use super::prelude::*;

pub fn dispatch(cpu: &mut CPU, opcode: u8) -> Result<CyclesType> {
    match opcode {
        // HALT
        0x76 => cpu.halt(),

        // STOP
        0x10 => cpu.stop(),

        // EI
        0xFB => cpu.enable_interrupts(),

        // DI
        0xF3 => cpu.disable_interrupts(),

        // RET Z - 如果零標誌設置則返回
        0xC8 => cpu.ret_z(),

        // CALL nn
        0xCD => cpu.call(),

        // JP nn
        0xC3 => cpu.jump(),

        // JR NZ,n
        0x20 => cpu.jr_nz(),

        // LD A,n
        0x3E => cpu.ld_a_n(),

        // ADD HL,DE
        0x19 => cpu.add_hl_de(),

        // LD (nn),A
        0xEA => cpu.ld_nn_a(),

        // NOP
        0x00 => {
            cpu.increment_pc(1)?;
            Ok(4) // NOP需要4個週期
        }

        // POP rr
        0xC1 => cpu.pop_bc(),
        0xD1 => cpu.pop_de(),
        0xE1 => cpu.pop_hl(),
        0xF1 => cpu.pop_af(),

        // PUSH rr
        0xC5 | 0xD5 | 0xE5 | 0xF5 => match opcode {
            0xC5 => cpu.push_bc(),
            0xD5 => cpu.push_de(),
            0xE5 => cpu.push_hl(),
            0xF5 => cpu.push_af(),
            _ => unreachable!(),
        },

        // RST n
        0xC7 | 0xCF | 0xD7 | 0xDF | 0xE7 | 0xEF | 0xF7 | 0xFF => {
            let address = match opcode {
                0xC7 => 0x00,
                0xCF => 0x08,
                0xD7 => 0x10,
                0xDF => 0x18,
                0xE7 => 0x20,
                0xEF => 0x28,
                0xF7 => 0x30,
                0xFF => 0x38,
                _ => unreachable!(),
            };
            cpu.rst(address)
        }

        // RET/RETI
        0xC9 => cpu.ret(),
        0xD9 => cpu.reti(),

        _ => Err(Error::Instruction(InstructionError::InvalidOpcode(opcode))),
    }
}

// 定義控制指令相關的特性
pub trait ControlInstructions {
    fn halt(&mut self) -> Result<CyclesType>;
    fn stop(&mut self) -> Result<CyclesType>;
    fn enable_interrupts(&mut self) -> Result<CyclesType>;
    fn disable_interrupts(&mut self) -> Result<CyclesType>;
    fn jump(&mut self) -> Result<CyclesType>;
    fn call(&mut self) -> Result<CyclesType>;
    fn ret(&mut self) -> Result<CyclesType>;
    fn reti(&mut self) -> Result<CyclesType>;
    fn rst(&mut self, addr: u16) -> Result<CyclesType>;
    fn call_conditional(&mut self, condition: u8) -> Result<CyclesType>;
    fn ret_conditional(&mut self, condition: u8) -> Result<CyclesType>;
    fn push_af(&mut self) -> Result<CyclesType>;
    fn push_bc(&mut self) -> Result<CyclesType>;
    fn push_de(&mut self) -> Result<CyclesType>;
    fn push_hl(&mut self) -> Result<CyclesType>;
    fn pop_af(&mut self) -> Result<CyclesType>;
    fn pop_bc(&mut self) -> Result<CyclesType>;
    fn pop_de(&mut self) -> Result<CyclesType>;
    fn pop_hl(&mut self) -> Result<CyclesType>;
}
