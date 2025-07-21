use crate::core::cpu::registers::Registers;
use crate::core::mmu::MMU;
use crate::utils::logger::log_to_file;

pub struct CPU {
    pub ime: bool,
    pub registers: Registers,
    pub mmu: std::rc::Rc<std::cell::RefCell<MMU>>,
}

impl CPU {
    // LD r, n : 將立即值 n 寫入指定暫存器
    pub fn ld_r_n(
        &mut self,
        target: crate::core::cpu::register_utils::RegTarget,
    ) -> crate::core::error::Result<crate::core::cycles::CyclesType> {
        let value = self.fetch_byte()?;
        self.set_reg_value(target, value);
        log_to_file(&format!(
            "[CPU] LD r, n: target={:?}, value={:#04X}",
            target, value
        ));
        Ok(crate::core::cycles::CYCLES_2)
    }

    // LD (HL+), A : 將 A 寫入 HL 指向記憶體，HL++
    pub fn ld_hli_a(&mut self) -> crate::core::error::Result<crate::core::cycles::CyclesType> {
        let hl = self.registers.get_hl();
        let value = self.registers.a;
        self.write_byte(hl, value)?;
        self.registers.set_hl(hl.wrapping_add(1));
        log_to_file(&format!(
            "[CPU] LD (HL+), A: HL={:#06X}, value={:#04X}",
            hl, value
        ));
        Ok(crate::core::cycles::CYCLES_2)
    }

    // LD HL, SP+r8 : HL = SP + signed r8
    pub fn ld_hl_sp_r8(&mut self) -> crate::core::error::Result<crate::core::cycles::CyclesType> {
        let sp = self.registers.sp;
        let r8 = self.fetch_byte()? as i8 as i16;
        let hl = (sp as i16).wrapping_add(r8) as u16;
        self.registers.set_hl(hl);
        log_to_file(&format!(
            "[CPU] LD HL, SP+r8: SP={:#06X}, r8={:+}, HL={:#06X}",
            sp, r8, hl
        ));
        Ok(crate::core::cycles::CYCLES_3)
    }

    // LD (nn), SP : 將 SP 寫入絕對位址 nn
    pub fn ld_nn_sp(&mut self) -> crate::core::error::Result<crate::core::cycles::CyclesType> {
        let addr = self.fetch_word()?;
        let sp = self.registers.sp;
        self.write_byte(addr, (sp & 0xFF) as u8)?;
        self.write_byte(addr + 1, (sp >> 8) as u8)?;
        log_to_file(&format!(
            "[CPU] LD (nn), SP: addr={:#06X}, SP={:#06X}",
            addr, sp
        ));
        Ok(crate::core::cycles::CYCLES_4)
    }
    // LD (HL), n : 將立即值 n 寫入 HL 指向的記憶體 (通常 VRAM)
    pub fn ld_hl_n(&mut self) -> crate::core::error::Result<crate::core::cycles::CyclesType> {
        let hl = self.registers.get_hl();
        let n = self.fetch_byte()?;
        self.write_byte(hl, n)?;
        log_to_file(&format!("[CPU] LD (HL), n: HL={:#06X}, n={:#04X}", hl, n));
        log_to_file(&format!(
            "[TRACE] LD (HL), n executed: PC={:#06X}, HL={:#06X}, n={:#04X}",
            self.registers.pc, hl, n
        ));
        Ok(crate::core::cycles::CYCLES_3)
    }

    // LD (HL), r : 將暫存器 r 的值寫入 HL 指向的記憶體 (通常 VRAM)
    pub fn ld_hl_r(
        &mut self,
        r: crate::core::cpu::register_utils::RegTarget,
    ) -> crate::core::error::Result<crate::core::cycles::CyclesType> {
        let hl = self.registers.get_hl();
        let value = self.registers.get_reg(r);
        self.write_byte(hl, value)?;
        log_to_file(&format!(
            "[CPU] LD (HL), r: HL={:#06X}, r={:?}, value={:#04X}",
            hl, r, value
        ));
        Ok(crate::core::cycles::CYCLES_2)
    }

    // LD (a16), A : 將 A 暫存器的值寫入 16-bit 絕對位址 (通常 VRAM)
    pub fn ld_a16_a(&mut self) -> crate::core::error::Result<crate::core::cycles::CyclesType> {
        let addr = self.fetch_word()?;
        let value = self.registers.a;
        self.write_byte(addr, value)?;
        log_to_file(&format!(
            "[CPU] LD (a16), A: addr={:#06X}, value={:#04X}",
            addr, value
        ));
        log_to_file(&format!(
            "[TRACE] LD (a16), A executed: PC={:#06X}, a16={:#06X}, value={:#04X}",
            self.registers.pc, addr, value
        ));
        Ok(crate::core::cycles::CYCLES_4)
    }

    // ...existing code...
    /// 設定暫存器值（供 bit 指令族使用）
    pub fn set_reg_value(&mut self, reg: super::register_utils::RegTarget, value: u8) {
        match reg {
            super::register_utils::RegTarget::A => self.registers.a = value,
            super::register_utils::RegTarget::B => self.registers.b = value,
            super::register_utils::RegTarget::C => self.registers.c = value,
            super::register_utils::RegTarget::D => self.registers.d = value,
            super::register_utils::RegTarget::E => self.registers.e = value,
            super::register_utils::RegTarget::H => self.registers.h = value,
            super::register_utils::RegTarget::L => self.registers.l = value,
            super::register_utils::RegTarget::HL => {
                let addr = self.registers.get_hl();
                let _ = self.write_byte(addr, value);
            }
            _ => {}
        }
    }
    #[allow(dead_code)]
    pub fn rlc_r(
        &mut self,
        _target: super::register_utils::RegTarget,
    ) -> crate::core::cycles::CyclesType {
        8
    }
    #[allow(dead_code)]
    pub fn rrc_r(
        &mut self,
        _target: super::register_utils::RegTarget,
    ) -> crate::core::cycles::CyclesType {
        8
    }
    #[allow(dead_code)]
    pub fn rl_r(
        &mut self,
        _target: super::register_utils::RegTarget,
    ) -> crate::core::cycles::CyclesType {
        8
    }
    #[allow(dead_code)]
    pub fn rr_r(
        &mut self,
        _target: super::register_utils::RegTarget,
    ) -> crate::core::cycles::CyclesType {
        8
    }
    #[allow(dead_code)]
    pub fn sla_r(
        &mut self,
        _target: super::register_utils::RegTarget,
    ) -> crate::core::cycles::CyclesType {
        8
    }
    #[allow(dead_code)]
    pub fn sra_r(
        &mut self,
        _target: super::register_utils::RegTarget,
    ) -> crate::core::cycles::CyclesType {
        8
    }
    #[allow(dead_code)]
    pub fn swap_r(
        &mut self,
        _target: super::register_utils::RegTarget,
    ) -> crate::core::cycles::CyclesType {
        8
    }
    #[allow(dead_code)]
    pub fn srl_r(
        &mut self,
        _target: super::register_utils::RegTarget,
    ) -> crate::core::cycles::CyclesType {
        8
    }
    #[allow(dead_code)]
    pub fn bit_b_r(
        &mut self,
        _bit: u8,
        _target: super::register_utils::RegTarget,
    ) -> crate::core::cycles::CyclesType {
        8
    }
    #[allow(dead_code)]
    pub fn res_b_r(
        &mut self,
        _bit: u8,
        _target: super::register_utils::RegTarget,
    ) -> crate::core::cycles::CyclesType {
        8
    }
    #[allow(dead_code)]
    pub fn set_b_r(
        &mut self,
        _bit: u8,
        _target: super::register_utils::RegTarget,
    ) -> crate::core::cycles::CyclesType {
        8
    }
    pub fn pop_word(&mut self) -> crate::core::error::Result<u16> {
        // TODO: 實作堆疊彈出
        Ok(0)
    }
    pub fn push_word(&mut self, value: u16) -> crate::core::error::Result<()> {
        // TODO: 實作堆疊推入
        Ok(())
    }
    pub fn pop_bc(&mut self) -> crate::core::error::Result<crate::core::cycles::CyclesType> {
        Ok(crate::core::cycles::CYCLES_1)
    }
    pub fn pop_de(&mut self) -> crate::core::error::Result<crate::core::cycles::CyclesType> {
        Ok(crate::core::cycles::CYCLES_1)
    }
    pub fn pop_hl(&mut self) -> crate::core::error::Result<crate::core::cycles::CyclesType> {
        Ok(crate::core::cycles::CYCLES_1)
    }
    pub fn pop_af(&mut self) -> crate::core::error::Result<crate::core::cycles::CyclesType> {
        Ok(crate::core::cycles::CYCLES_1)
    }
    pub fn push_bc(&mut self) -> crate::core::error::Result<crate::core::cycles::CyclesType> {
        Ok(crate::core::cycles::CYCLES_1)
    }
    pub fn push_de(&mut self) -> crate::core::error::Result<crate::core::cycles::CyclesType> {
        Ok(crate::core::cycles::CYCLES_1)
    }
    pub fn push_hl(&mut self) -> crate::core::error::Result<crate::core::cycles::CyclesType> {
        Ok(crate::core::cycles::CYCLES_1)
    }
    pub fn push_af(&mut self) -> crate::core::error::Result<crate::core::cycles::CyclesType> {
        Ok(crate::core::cycles::CYCLES_1)
    }
    /// 取得可變暫存器引用
    pub fn registers_mut(&mut self) -> &mut Registers {
        &mut self.registers
    }

    /// 取得不可變暫存器引用
    pub fn registers(&self) -> &Registers {
        &self.registers
    }
    pub fn new(mmu: std::rc::Rc<std::cell::RefCell<MMU>>) -> Self {
        CPU {
            ime: false,
            registers: Registers::new(),
            mmu,
        }
    }
    /// 取得 MMU 參考
    pub fn mmu(&self) -> &std::rc::Rc<std::cell::RefCell<MMU>> {
        &self.mmu
    }

    // ...existing code...

    pub fn fetch_word(&mut self) -> crate::core::error::Result<u16> {
        // 以 PC 為位址連續讀取兩個 byte（小端序），並自動遞增 PC
        let low = self.fetch_byte()? as u16;
        let high = self.fetch_byte()? as u16;
        Ok((high << 8) | low)
    }

    pub fn read_byte(&mut self, addr: u16) -> crate::core::error::Result<u8> {
        self.mmu.borrow().read_byte(addr)
    }

    /// 寫入一個 byte 到記憶體
    pub fn write_byte(&mut self, addr: u16, value: u8) -> crate::core::error::Result<()> {
        self.mmu.borrow_mut().write_byte(addr, value)
    }

    pub fn and_a_r(
        &mut self,
        _target: super::register_utils::RegTarget,
    ) -> crate::core::error::Result<crate::core::cycles::CyclesType> {
        // TODO: 實作 AND A,r
        Ok(4)
    }
    pub fn or_a_r(
        &mut self,
        _target: super::register_utils::RegTarget,
    ) -> crate::core::error::Result<crate::core::cycles::CyclesType> {
        // TODO: 實作 OR A,r
        Ok(4)
    }
    pub fn cpl(&mut self) -> crate::core::error::Result<crate::core::cycles::CyclesType> {
        // TODO: 實作 CPL
        Ok(4)
    }
    pub fn scf(&mut self) -> crate::core::error::Result<crate::core::cycles::CyclesType> {
        // TODO: 實作 SCF
        Ok(4)
    }
    pub fn ccf(&mut self) -> crate::core::error::Result<crate::core::cycles::CyclesType> {
        // TODO: 實作 CCF
        Ok(4)
    }
    pub fn daa(&mut self) -> crate::core::error::Result<crate::core::cycles::CyclesType> {
        // TODO: 實作 DAA
        Ok(4)
    }

    // 補齊 fetch_byte stub function
    pub fn fetch_byte(&mut self) -> Result<u8, crate::core::error::Error> {
        // 真正從 MMU 讀取 PC 指向的 byte，並自動遞增 PC
        let pc_before = self.registers.pc;
        let byte = self.mmu.borrow().read_byte(self.registers.pc)?;
        self.registers.pc = self.registers.pc.wrapping_add(1);
        log_to_file(&format!(
            "[fetch_byte] PC={:04X} -> {:04X}, opcode={:02X}",
            pc_before, self.registers.pc, byte
        ));
        Ok(byte)
    }

    // arithmetic.rs 相關 stub
    pub fn dec_r(
        &mut self,
        _target: super::register_utils::RegTarget,
    ) -> crate::core::cycles::CyclesType {
        0
    }
    pub fn add_a_r(
        &mut self,
        _target: super::register_utils::RegTarget,
        _use_carry: bool,
    ) -> crate::core::cycles::CyclesType {
        0
    }
    pub fn sub_a_r(
        &mut self,
        _target: super::register_utils::RegTarget,
        _use_carry: bool,
    ) -> crate::core::cycles::CyclesType {
        0
    }
    pub fn add_a_n(&mut self, _use_carry: bool) -> crate::core::cycles::CyclesType {
        0
    }
    pub fn sub_a_n(&mut self, _use_carry: bool) -> crate::core::cycles::CyclesType {
        0
    }

    // cb.rs 相關 stub
    pub fn cb_misc(
        &mut self,
        _target: super::register_utils::RegTarget,
    ) -> crate::core::cycles::CyclesType {
        0
    }

    // control.rs 相關 stub

    // io.rs 相關 stub
    pub fn read_joypad(&mut self) -> crate::core::cycles::CyclesType {
        0
    }
    pub fn write_joypad(&mut self) -> crate::core::cycles::CyclesType {
        0
    }
    pub fn read_serial(&mut self) -> crate::core::cycles::CyclesType {
        0
    }
    pub fn write_serial(&mut self) -> crate::core::cycles::CyclesType {
        0
    }

    // stack.rs 相關 stub
    pub fn push_r(
        &mut self,
        _target: super::register_utils::RegTarget,
    ) -> crate::core::cycles::CyclesType {
        0
    }
    pub fn pop_r(
        &mut self,
        _target: super::register_utils::RegTarget,
    ) -> crate::core::cycles::CyclesType {
        0
    }
}
