use std::error::Error as StdError;
use std::fmt;

#[derive(Debug)]
pub enum Error {
    Instruction(InstructionError),
    Hardware(HardwareError),
    Video(String),
    Config(String),
    Audio(String),
    IO(std::io::Error),
}

#[derive(Debug)]
pub enum InstructionError {
    InvalidOpcode(u8),
    InvalidRegister(super::cpu::register_target::RegTarget),
    InvalidRegisterPair(super::cpu::register_target::RegTarget),
    InvalidCondition(u8),
    Custom(String),
}

#[derive(Debug)]
pub enum HardwareError {
    MemoryOutOfBounds(u16),
    InvalidInterrupt,
    InvalidRegisterAccess,
    BorrowError,
    InvalidROMSize,
    InvalidROM,
    PPU(String),
    Timer(String),
    Config(String),
    Audio(String),
}

impl fmt::Display for Error {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        match self {
            Error::Instruction(e) => write!(f, "指令錯誤: {:?}", e),
            Error::Hardware(e) => write!(f, "硬體錯誤: {:?}", e),
            Error::Video(e) => write!(f, "顯示錯誤: {}", e),
            Error::Config(e) => write!(f, "設定錯誤: {}", e),
            Error::Audio(e) => write!(f, "音頻錯誤: {}", e),
            Error::IO(e) => write!(f, "IO 錯誤: {}", e),
        }
    }
}

impl StdError for Error {}

pub type Result<T> = std::result::Result<T, Error>;

// 實現錯誤轉換
impl From<std::io::Error> for Error {
    fn from(err: std::io::Error) -> Self {
        Error::IO(err)
    }
}
