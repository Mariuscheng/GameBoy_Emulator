pub struct MMU {
    // 你可以在這裡加上記憶體欄位，例如：
    pub memory: [u8; 0x10000],              // 64KB GameBoy 記憶體
    pub vram: crate::core::ppu::vram::VRAM, // 8KB VRAM
}

impl MMU {
    pub fn new() -> Self {
        MMU {
            memory: [0; 0x10000],
            vram: crate::core::ppu::vram::VRAM::new(),
        }
    }
    /// 寫入 GameBoy 記憶體
    pub fn write_byte(&mut self, addr: u16, value: u8) {
        if (0x8000..=0x9FFF).contains(&addr) {
            self.vram.write_byte((addr - 0x8000) as usize, value);
            crate::utils::logger::log_to_file(&format!(
                "[MMU] write_byte to VRAM: addr=0x{:04X}, value=0x{:02X}",
                addr, value
            ));
        } else {
            self.memory[addr as usize] = value;
            crate::utils::logger::log_to_file(&format!(
                "[MMU] write_byte: addr=0x{:04X}, value=0x{:02X}",
                addr, value
            ));
        }
    }
    /// 讀取 GameBoy 記憶體
    pub fn read_byte(&self, addr: u16) -> u8 {
        if (0x8000..=0x9FFF).contains(&addr) {
            self.vram.read_byte((addr - 0x8000) as usize)
        } else {
            self.memory[addr as usize]
        }
    }
}
