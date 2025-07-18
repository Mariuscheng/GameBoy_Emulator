// Common exports for CPU instruction modules
pub use super::register_utils::FlagOperations;
pub use crate::core::cpu::CPU;
pub use crate::core::cpu::register_target::RegTarget;
pub use crate::core::cycles::{CYCLES_1, CYCLES_2, CYCLES_3, CYCLES_4, CyclesType};

// Error types
pub use crate::core::error::{Error, HardwareError, InstructionError, Result};

// Common cycle constants
pub const CB_PREFIX_CYCLES: CyclesType = CYCLES_2;
pub const JUMP_CYCLES: CyclesType = CYCLES_3;
pub const CALL_CYCLES: CyclesType = CYCLES_4;

// Define utility functions for flag operations
pub trait FlagUtils {
    fn update_zero_flag(&mut self, value: u8);
    fn update_carry_flag(&mut self, value: bool);
    fn update_half_carry_flag(&mut self, value: bool);
    fn update_subtract_flag(&mut self, value: bool);
}

impl FlagUtils for CPU {
    fn update_zero_flag(&mut self, value: u8) {
        self.registers.set_zero(value == 0);
    }

    fn update_carry_flag(&mut self, value: bool) {
        self.registers.set_carry(value);
    }

    fn update_half_carry_flag(&mut self, value: bool) {
        self.registers.set_half_carry(value);
    }

    fn update_subtract_flag(&mut self, value: bool) {
        self.registers.set_subtract(value);
    }
}
