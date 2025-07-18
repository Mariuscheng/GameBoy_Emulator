use crate::core::error::{Error, InstructionError, Result};

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum RegTarget {
    A,     // 累加器
    B,     // B 寄存器
    C,     // C 寄存器
    D,     // D 寄存器
    E,     // E 寄存器
    H,     // H 寄存器
    L,     // L 寄存器
    HL,    // HL 暫存器對
    MemHL, // HL 暫存器對指向的記憶體位置
    SP,    // 堆疊指標
    PC,    // 程式計數器
    BC,    // BC 暫存器對
    DE,    // DE 暫存器對
    AF,    // AF 暫存器對
}

impl RegTarget {
    pub fn from_bits(bits: u8) -> Result<RegTarget> {
        match bits & 0x07 {
            0b000 => Ok(RegTarget::B),
            0b001 => Ok(RegTarget::C),
            0b010 => Ok(RegTarget::D),
            0b011 => Ok(RegTarget::E),
            0b100 => Ok(RegTarget::H),
            0b101 => Ok(RegTarget::L),
            0b110 => Ok(RegTarget::HL),
            0b111 => Ok(RegTarget::A),
            _ => Err(Error::Instruction(InstructionError::InvalidRegister(
                RegTarget::A,
            ))),
        }
    }

    pub fn is_16bit(&self) -> bool {
        matches!(
            self,
            RegTarget::BC
                | RegTarget::DE
                | RegTarget::HL
                | RegTarget::SP
                | RegTarget::PC
                | RegTarget::AF
        )
    }

    pub fn is_8bit(&self) -> bool {
        !self.is_16bit()
    }
}
