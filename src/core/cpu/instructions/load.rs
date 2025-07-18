use super::prelude::*;
use super::register_utils::FlagOperations;
use crate::core::cycles::CYCLES_5;

/// 處理 LD 指令族
pub fn dispatch(cpu: &mut CPU, opcode: u8) -> Result<CyclesType> {
    match opcode {
        // LD BC, nn
        0x01 => cpu.ld_bc_nn(),

        // LD DE, nn
        0x11 => cpu.ld_de_nn(),

        // LD HL, nn
        0x21 => cpu.ld_hl_nn(),

        // LD SP, nn
        0x31 => cpu.ld_sp_nn(),

        // LD (HL), r
        0x70..=0x77 => {
            let src = opcode & 0x07;
            let source = RegTarget::from_bits(src)?;
            cpu.ld_hl_r(source)
        }

        // LD r, r'
        0x40..=0x7F => {
            let dst = ((opcode >> 3) & 0x07) as u8;
            let src = (opcode & 0x07) as u8;
            let target = RegTarget::from_bits(dst)?;
            let source = RegTarget::from_bits(src)?;
            cpu.ld_r_r(target, source)
        }

        // LD r, n
        0x06 | 0x0E | 0x16 | 0x1E | 0x26 | 0x2E | 0x3E => {
            let reg = ((opcode >> 3) & 0x07) as u8;
            let target = RegTarget::from_bits(reg)?;
            cpu.ld_r_n(target)
        }

        // LD A, (BC)
        0x0A => cpu.ld_a_bc(),

        // LD A, (DE)
        0x1A => cpu.ld_a_de(),

        // LD A, (nn)
        0xFA => cpu.ld_a_nn(),

        // LD (BC), A
        0x02 => cpu.ld_bc_a(),

        // LD (DE), A
        0x12 => cpu.ld_de_a(),

        // LD (nn), A
        0xEA => cpu.ld_nn_a(),

        // LD A, (C)
        0xF2 => cpu.ld_a_c(),

        // LD (C), A
        0xE2 => cpu.ld_c_a(),

        // LDH (n), A
        0xE0 => cpu.ldh_n_a(),

        // LDH A, (n)
        0xF0 => cpu.ldh_a_n(),

        // LD (HL+), A
        0x22 => cpu.ld_hli_a(),

        // LD A, (HL+)
        0x2A => cpu.ld_a_hli(),

        // LD (HL-), A
        0x32 => cpu.ld_hld_a(),

        // LD A, (HL-)
        0x3A => cpu.ld_a_hld(), // LD (HL), n
        0x36 => cpu.ld_hl_n(),

        // LD SP, HL
        0xF9 => cpu.ld_sp_hl(),

        // LD HL, SP+r8
        0xF8 => cpu.ld_hl_sp_r8(),

        // LD (nn),SP
        0x08 => cpu.ld_nn_sp(),

        _ => Err(Error::Instruction(InstructionError::InvalidOpcode(opcode))),
    }
}

/// 實作 LD 指令相關方法
impl CPU {
    // LD r, r' - 將寄存器 r' 的值載入到寄存器 r
    pub fn ld_r_r(&mut self, target: RegTarget, source: RegTarget) -> Result<CyclesType> {
        let value = self.registers.get_register(&source)?;
        self.registers.set_register(&target, value)?;
        let pc = self.registers.get_pc();
        self.registers.set_pc(pc.wrapping_add(1));
        Ok(CYCLES_1)
    }

    // LD r, n - 將立即數 n 載入到寄存器 r
    pub fn ld_r_n(&mut self, target: RegTarget) -> Result<CyclesType> {
        let value = self.fetch_byte()?;
        self.registers.set_register(&target, value)?;
        Ok(CYCLES_2)
    }

    // LD r, (HL) - 將 (HL) 位址的值載入到寄存器 r
    pub fn ld_r_hl(&mut self, target: RegTarget) -> Result<CyclesType> {
        let addr = self.registers.get_hl();
        let value = self.read_byte(addr)?;
        self.registers.set_register(&target, value)?;
        Ok(CYCLES_2)
    }

    // LD (HL), r - 將寄存器 r 的值寫入到 (HL) 位址
    pub fn ld_hl_r(&mut self, source: RegTarget) -> Result<CyclesType> {
        let value = self.registers.get_register(&source)?;
        let addr = self.registers.get_hl();
        self.write_byte(addr, value)?;
        Ok(CYCLES_2)
    }

    // LD A, (BC) - 將 (BC) 位址的值載入到 A
    pub fn ld_a_bc(&mut self) -> Result<CyclesType> {
        let addr = self.registers.get_bc();
        let value = self.read_byte(addr)?;
        self.registers.set_a(value);
        Ok(CYCLES_2)
    }

    // LD (BC), A - 將 A 的值寫入到 (BC) 位址
    pub fn ld_bc_a(&mut self) -> Result<CyclesType> {
        let addr = self.registers.get_bc();
        self.write_byte(addr, self.registers.get_a())?;
        Ok(CYCLES_2)
    }

    // LD A, (DE) - 將 (DE) 位址的值載入到 A
    pub fn ld_a_de(&mut self) -> Result<CyclesType> {
        let addr = self.registers.get_de();
        let value = self.read_byte(addr)?;
        self.registers.set_a(value);
        Ok(CYCLES_2)
    }

    // LD (DE), A - 將 A 的值寫入到 (DE) 位址
    pub fn ld_de_a(&mut self) -> Result<CyclesType> {
        let addr = self.registers.get_de();
        self.write_byte(addr, self.registers.get_a())?;
        Ok(CYCLES_2)
    }

    // LDH (n), A - 將 A 的值寫入到 (0xFF00 + n) 位址
    pub fn ldh_n_a(&mut self) -> Result<CyclesType> {
        let n = self.fetch_byte()?;
        let addr = 0xFF00 | (n as u16);
        self.write_byte(addr, self.registers.get_a())?;
        Ok(CYCLES_3)
    }

    // LDH A, (n) - 將 (0xFF00 + n) 位址的值載入到 A
    pub fn ldh_a_n(&mut self) -> Result<CyclesType> {
        let n = self.fetch_byte()?;
        let addr = 0xFF00 | (n as u16);
        let value = self.read_byte(addr)?;
        self.registers.set_a(value);
        Ok(CYCLES_3)
    }

    // LD SP, nn - 將立即數 nn 載入到 SP
    pub fn ld_sp_nn(&mut self) -> Result<CyclesType> {
        let nn = self.fetch_word()?;
        self.registers.set_sp(nn);
        Ok(CYCLES_3)
    }

    // LD SP, HL - 將 HL 的值載入到 SP
    pub fn ld_sp_hl(&mut self) -> Result<CyclesType> {
        let value = self.registers.get_hl();
        self.registers.set_sp(value);
        Ok(CYCLES_2)
    }

    // LD HL, SP+n - 將 SP+n 的值載入到 HL
    pub fn ldhl_sp_n(&mut self) -> Result<CyclesType> {
        let n = self.fetch_byte()? as i8;
        let sp = self.registers.get_sp();
        let result = ((sp as i32) + (n as i32)) as u16;

        // 設置標誌位
        let half_carry = (sp & 0xF) + (n as u16 & 0xF) > 0xF;
        let carry = (sp & 0xFF) + (n as u16 & 0xFF) > 0xFF;

        self.registers.set_hl(result);
        self.registers.set_zero(false);
        self.registers.set_subtract(false);
        self.registers.set_half_carry(half_carry);
        self.registers.set_carry(carry);

        Ok(CYCLES_3)
    }
    pub fn jump(&mut self) -> Result<CyclesType> {
        let addr = self.fetch_word()?;
        self.registers.set_pc(addr);
        Ok(CYCLES_3)
    }

    // LD BC, nn - 將立即數 nn 載入到 BC
    pub fn ld_bc_nn(&mut self) -> Result<CyclesType> {
        let nn = self.fetch_word()?;
        self.registers.set_bc(nn);
        Ok(CYCLES_3)
    }

    // LD DE, nn - 將立即數 nn 載入到 DE
    pub fn ld_de_nn(&mut self) -> Result<CyclesType> {
        let nn = self.fetch_word()?;
        self.registers.set_de(nn);
        Ok(CYCLES_3)
    }

    // LD HL, nn - 將立即數 nn 載入到 HL
    pub fn ld_hl_nn(&mut self) -> Result<CyclesType> {
        let nn = self.fetch_word()?;
        self.registers.set_hl(nn);
        Ok(CYCLES_3)
    }

    // LD A, (nn) - 將位址 nn 的值載入到 A
    pub fn ld_a_nn(&mut self) -> Result<CyclesType> {
        let addr = self.fetch_word()?;
        let value = self.read_byte(addr)?;
        self.registers.set_a(value);
        Ok(CYCLES_4)
    }

    // LD (nn), A - 將 A 的值寫入到位址 nn
    pub fn ld_nn_a(&mut self) -> Result<CyclesType> {
        let addr = self.fetch_word()?;
        self.write_byte(addr, self.registers.get_a())?;
        Ok(CYCLES_4)
    }

    // LD (nn), SP - 將 SP 的值寫入到位址 nn
    pub fn ld_nn_sp(&mut self) -> Result<CyclesType> {
        let addr = self.fetch_word()?;
        let sp = self.registers.get_sp();
        self.write_word(addr, sp)?;
        Ok(CYCLES_5)
    }

    // LD A, (C) - 將 (0xFF00 + C) 位址的值載入到 A
    pub fn ld_a_c(&mut self) -> Result<CyclesType> {
        let addr = 0xFF00 | (self.registers.get_c() as u16);
        let value = self.read_byte(addr)?;
        self.registers.set_a(value);
        Ok(CYCLES_2)
    }

    // LD (C), A - 將 A 的值寫入到 (0xFF00 + C) 位址
    pub fn ld_c_a(&mut self) -> Result<CyclesType> {
        let addr = 0xFF00 | (self.registers.get_c() as u16);
        self.write_byte(addr, self.registers.get_a())?;
        Ok(CYCLES_2)
    }

    // LD (HLI), A - 將 A 的值寫入到 HL 位址，然後 HL++
    pub fn ld_hli_a(&mut self) -> Result<CyclesType> {
        let addr = self.registers.get_hl();
        self.write_byte(addr, self.registers.get_a())?;
        self.registers.set_hl(addr.wrapping_add(1));
        Ok(CYCLES_2)
    }

    // LD A, (HLI) - 將 HL 位址的值載入到 A，然後 HL++
    pub fn ld_a_hli(&mut self) -> Result<CyclesType> {
        let addr = self.registers.get_hl();
        let value = self.read_byte(addr)?;
        self.registers.set_a(value);
        self.registers.set_hl(addr.wrapping_add(1));
        Ok(CYCLES_2)
    }

    // LD (HLD), A - 將 A 的值寫入到 HL 位址，然後 HL--
    pub fn ld_hld_a(&mut self) -> Result<CyclesType> {
        let addr = self.registers.get_hl();
        self.write_byte(addr, self.registers.get_a())?;
        self.registers.set_hl(addr.wrapping_sub(1));
        Ok(CYCLES_2)
    }

    // LD A, (HLD) - 將 HL 位址的值載入到 A，然後 HL--
    pub fn ld_a_hld(&mut self) -> Result<CyclesType> {
        let addr = self.registers.get_hl();
        let value = self.read_byte(addr)?;
        self.registers.set_a(value);
        self.registers.set_hl(addr.wrapping_sub(1));
        Ok(CYCLES_2)
    }

    // LD (HL), n - 將立即數 n 寫入到 HL 位址
    pub fn ld_hl_n(&mut self) -> Result<CyclesType> {
        let value = self.fetch_byte()?;
        let addr = self.registers.get_hl();
        self.write_byte(addr, value)?;
        Ok(CYCLES_3)
    }

    // LDHL SP, r8 - 將 SP + r8 的值載入到 HL
    pub fn ld_hl_sp_r8(&mut self) -> Result<CyclesType> {
        let r8 = self.fetch_byte()? as i8;
        let sp = self.registers.get_sp();
        let result = ((sp as i32) + (r8 as i32)) as u16;

        let half_carry = (sp & 0xF) + (r8 as u16 & 0xF) > 0xF;
        let carry = (sp & 0xFF) + (r8 as u16 & 0xFF) > 0xFF;

        self.registers.set_hl(result);
        self.registers.set_zero(false);
        self.registers.set_subtract(false);
        self.registers.set_half_carry(half_carry);
        self.registers.set_carry(carry);

        Ok(CYCLES_3)
    }

    // write_word - 輔助方法，將 16 位元值寫入記憶體
    fn write_word(&mut self, addr: u16, value: u16) -> Result<()> {
        self.write_byte(addr, (value & 0xFF) as u8)?;
        self.write_byte(addr.wrapping_add(1), (value >> 8) as u8)?;
        Ok(())
    }
}
