//! PPU VRAM 操作
use crate::utils::logger::log_to_file;

pub struct VRAM {
    pub data: [u8; 0x2000], // 8KB
}

impl VRAM {
    /// 建立新的 VRAM 實例
    pub fn new() -> Self {
        VRAM { data: [0; 0x2000] }
    }
    /// 寫入 VRAM 指定位址
    pub fn write_byte(&mut self, addr: usize, value: u8) {
        if addr < 0x2000 {
            self.data[addr] = value;
            log_to_file(&format!(
                "[VRAM] write_byte: addr={:04X}, value={:02X}",
                addr, value
            ));
        } else {
            // ...existing code...
        }
    }
    /// 讀取 VRAM 指定位址
    pub fn read_byte(&self, addr: usize) -> u8 {
        if addr < 0x2000 {
            let value = self.data[addr];
            // ...existing code...
            log_to_file(&format!(
                "[VRAM] read_byte: addr={:04X}, value={:02X}",
                addr, value
            ));
            value
        } else {
            // ...existing code...
            0
        }
    }
    /// 清空 VRAM
    pub fn clear(&mut self) {
        self.data.fill(0);
    }
}

pub fn clear_vram(vram: &mut [u8]) {
    vram.fill(0);
}
