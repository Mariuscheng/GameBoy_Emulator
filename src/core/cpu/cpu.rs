use crate::core::cpu::registers::Registers;
use crate::core::mmu::MMU;

pub struct CPU {
    pub halted: bool,
    pub ime: bool,
    pub registers: Registers,
    pub mmu: std::rc::Rc<std::cell::RefCell<MMU>>,
    // 補齊 timer 欄位
    pub tima: u8,
    pub tma: u8,
}

impl CPU {
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
    // bit.rs 指令族 stub
    pub fn rlc_r(
        &mut self,
        _target: super::register_utils::RegTarget,
    ) -> crate::core::cycles::CyclesType {
        8
    }
    pub fn rrc_r(
        &mut self,
        _target: super::register_utils::RegTarget,
    ) -> crate::core::cycles::CyclesType {
        8
    }
    pub fn rl_r(
        &mut self,
        _target: super::register_utils::RegTarget,
    ) -> crate::core::cycles::CyclesType {
        8
    }
    pub fn rr_r(
        &mut self,
        _target: super::register_utils::RegTarget,
    ) -> crate::core::cycles::CyclesType {
        8
    }
    pub fn sla_r(
        &mut self,
        _target: super::register_utils::RegTarget,
    ) -> crate::core::cycles::CyclesType {
        8
    }
    pub fn sra_r(
        &mut self,
        _target: super::register_utils::RegTarget,
    ) -> crate::core::cycles::CyclesType {
        8
    }
    pub fn swap_r(
        &mut self,
        _target: super::register_utils::RegTarget,
    ) -> crate::core::cycles::CyclesType {
        8
    }
    pub fn srl_r(
        &mut self,
        _target: super::register_utils::RegTarget,
    ) -> crate::core::cycles::CyclesType {
        8
    }
    pub fn bit_b_r(
        &mut self,
        _bit: u8,
        _target: super::register_utils::RegTarget,
    ) -> crate::core::cycles::CyclesType {
        8
    }
    pub fn res_b_r(
        &mut self,
        _bit: u8,
        _target: super::register_utils::RegTarget,
    ) -> crate::core::cycles::CyclesType {
        8
    }
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
            halted: false,
            ime: false,
            registers: Registers::new(),
            mmu,
            tima: 0,
            tma: 0,
        }
    }
    /// 取得 MMU 參考
    pub fn mmu(&self) -> &std::rc::Rc<std::cell::RefCell<MMU>> {
        &self.mmu
    }

    // 補齊缺失 stub function
    pub fn inc_r(
        &mut self,
        _target: super::register_utils::RegTarget,
    ) -> crate::core::cycles::CyclesType {
        // stub
        0
    }

    pub fn inc_hl(&mut self) -> crate::core::cycles::CyclesType {
        // stub
        0
    }

    pub fn dec_hl(&mut self) -> crate::core::cycles::CyclesType {
        // stub
        0
    }

    pub fn add_hl_rr(
        &mut self,
        _target: super::register_utils::RegTarget,
    ) -> crate::core::cycles::CyclesType {
        // stub
        0
    }

    pub fn add_sp_n(&mut self) -> crate::core::cycles::CyclesType {
        // stub
        0
    }

    pub fn sub_a(&mut self) -> crate::core::cycles::CyclesType {
        // stub
        0
    }

    pub fn sbc_a(&mut self) -> crate::core::cycles::CyclesType {
        // stub
        0
    }

    pub fn enable_interrupts(&mut self) -> crate::core::cycles::CyclesType {
        // stub
        0
    }

    pub fn disable_interrupts(&mut self) -> crate::core::cycles::CyclesType {
        // stub
        0
    }

    pub fn call_conditional(&mut self, _condition: u8) -> crate::core::cycles::CyclesType {
        // stub
        0
    }

    pub fn ret_conditional(&mut self, _condition: u8) -> crate::core::cycles::CyclesType {
        // stub
        0
    }

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
    // timer stub
    pub fn inc_tima(&mut self) -> crate::core::cycles::CyclesType {
        self.tima = self.tima.wrapping_add(1);
        4
    }
    pub fn dec_tima(&mut self) -> crate::core::cycles::CyclesType {
        self.tima = self.tima.wrapping_sub(1);
        4
    }
    pub fn set_tma(&mut self) -> crate::core::cycles::CyclesType {
        self.tma = self.tima;
        4
    }

    // 補齊 fetch_byte stub function
    pub fn fetch_byte(&mut self) -> Result<u8, crate::core::error::Error> {
        // stub
        Ok(0)
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
