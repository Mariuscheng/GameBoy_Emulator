// Error handling module
pub mod hardware;

use thiserror::Error;

// Re-exports
pub use self::hardware::HardwareError;

// Result type definition
pub type Result<T> = std::result::Result<T, Error>;

#[derive(Error, Debug)]
pub enum Error {
    #[error("Hardware error: {0}")]
    Hardware(#[from] HardwareError),

    #[error("Instruction error: {0}")]
    Instruction(#[from] InstructionError),

    #[error("IO error: {0}")]
    IO(#[from] std::io::Error),

    #[error("Configuration error: {0}")]
    Config(String),

    #[error("Audio error: {0}")]
    Audio(String),

    #[error("Display error: {0}")]
    Video(String),

    #[error("Memory error: {0}")]
    Memory(String),
}

use crate::core::cpu::register_target::RegTarget;

#[derive(Error, Debug)]
pub enum InstructionError {
    #[error("Invalid opcode: {0:02X}")]
    InvalidOpcode(u8),

    #[error("Invalid register pair: {0:02X}")]
    InvalidRegisterPair(u8),

    #[error("Invalid register: {0:?}")]
    InvalidRegister(RegTarget),

    #[error("Invalid condition: {0:02X}")]
    InvalidCondition(u8),

    #[error("Invalid instruction: {0}")]
    Custom(String),
}

impl From<std::cell::BorrowError> for Error {
    fn from(_: std::cell::BorrowError) -> Self {
        Error::Hardware(HardwareError::borrow_error("無法借用 MMU"))
    }
}

impl From<std::cell::BorrowMutError> for Error {
    fn from(_: std::cell::BorrowMutError) -> Self {
        Error::Hardware(HardwareError::borrow_error("無法可變借用 MMU"))
    }
}
