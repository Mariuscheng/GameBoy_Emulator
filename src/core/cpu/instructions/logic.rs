use super::prelude::*;
use super::register_utils::FlagOperations;

pub fn dispatch(cpu: &mut CPU, opcode: u8) -> Result<CyclesType> {
    match opcode {
        // AND r
        0xA0..=0xA7 => {
            let reg = opcode & 0x07;
            let target = RegTarget::from_bits(reg)?;
            cpu.and_a_r(target)
        }

        // AND n
        0xE6 => cpu.and_a_n(),

        // OR r
        0xB0..=0xB7 => {
            let reg = opcode & 0x07;
            let target = RegTarget::from_bits(reg)?;
            cpu.or_a_r(target)
        }

        // OR n
        0xF6 => cpu.or_a_n(),

        // XOR r
        0xA8..=0xAF => {
            let reg = opcode & 0x07;
            let target = RegTarget::from_bits(reg)?;
            cpu.xor_a_r(target)
        }

        // XOR n
        0xEE => cpu.xor_a_n(),

        // CP r
        0xB8..=0xBF => {
            let reg = opcode & 0x07;
            let target = RegTarget::from_bits(reg)?;
            cpu.cp_a_r(target)
        }

        // CP n
        0xFE => cpu.cp_a_n(),

        _ => Err(Error::Instruction(InstructionError::InvalidOpcode(opcode))),
    }
}

impl CPU {
    pub fn and_a_r(&mut self, target: RegTarget) -> Result<CyclesType> {
        let value = self.registers.get_register(&target)?;
        let a = self.registers.get_a();
        let result = a & value;
        self.registers.set_a(result);
        self.update_logic_flags(result, true);
        Ok(CYCLES_1)
    }

    pub fn and_a_n(&mut self) -> Result<CyclesType> {
        let value = self.fetch_byte()?;
        let a = self.registers.get_a();
        let result = a & value;
        self.registers.set_a(result);
        self.update_logic_flags(result, true);
        Ok(CYCLES_2)
    }

    pub fn or_a_r(&mut self, target: RegTarget) -> Result<CyclesType> {
        let value = self.registers.get_register(&target)?;
        let a = self.registers.get_a();
        let result = a | value;
        self.registers.set_a(result);
        self.update_logic_flags(result, false);
        Ok(CYCLES_1)
    }

    pub fn or_a_n(&mut self) -> Result<CyclesType> {
        let value = self.fetch_byte()?;
        let a = self.registers.get_a();
        let result = a | value;
        self.registers.set_a(result);
        self.update_logic_flags(result, false);
        Ok(CYCLES_2)
    }

    pub fn xor_a_r(&mut self, target: RegTarget) -> Result<CyclesType> {
        let value = self.registers.get_register(&target)?;
        let a = self.registers.get_a();
        let result = a ^ value;
        self.registers.set_a(result);
        self.update_logic_flags(result, false);
        Ok(CYCLES_1)
    }

    pub fn xor_a_n(&mut self) -> Result<CyclesType> {
        let value = self.fetch_byte()?;
        let a = self.registers.get_a();
        let result = a ^ value;
        self.registers.set_a(result);
        self.update_logic_flags(result, false);
        Ok(CYCLES_2)
    }

    // CP指令：與A寄存器比較，但不儲存結果
    pub fn cp_a_r(&mut self, target: RegTarget) -> Result<CyclesType> {
        let value = self.registers.get_register(&target)?;
        let a = self.registers.get_a();
        let result = a.wrapping_sub(value);

        // 設置標誌位
        self.registers.set_zero(result == 0);
        self.registers.set_subtract(true);
        self.registers.set_half_carry((a & 0x0F) < (value & 0x0F));
        self.registers.set_carry(a < value);

        self.increment_pc(1)?;
        Ok(4)
    }

    pub fn cp_a_n(&mut self) -> Result<CyclesType> {
        let value = self.fetch_byte()?;
        let a = self.registers.get_a();
        let result = a.wrapping_sub(value);

        // 設置標誌位
        self.registers.set_zero(result == 0);
        self.registers.set_subtract(true);
        self.registers.set_half_carry((a & 0x0F) < (value & 0x0F));
        self.registers.set_carry(a < value);

        self.increment_pc(1)?;
        Ok(8)
    }

    fn update_logic_flags(&mut self, result: u8, is_and: bool) {
        self.registers.set_zero(result == 0);
        self.registers.set_subtract(false);
        self.registers.set_half_carry(is_and);
        self.registers.set_carry(false);
    }
}
