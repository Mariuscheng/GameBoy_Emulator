pub mod core;
pub mod interface;
pub mod mmu;
pub mod utils;
pub mod error;
pub mod audio;
pub mod sdl3_display;
pub mod rom_check;
// If you want to re-export rom_header, use:
pub use crate::core::rom_header::*;