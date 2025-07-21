//! MBC5 記憶體銀行控制器 stub

#[derive(Debug, Clone)]
pub struct MBC5 {
    pub rom_bank: u16,     // 9-bit ROM bank
    pub ram_bank: u8,      // 4-bit RAM bank
    pub ram_enabled: bool, // RAM enable flag
    pub ram: [u8; 0x8000], // 32KB external RAM (max for MBC5)
}

impl MBC5 {
    pub fn new() -> Self {
        Self {
            rom_bank: 1,
            ram_bank: 0,
            ram_enabled: false,
            ram: [0; 0x8000],
        }
    }

    /// 讀取 MBC5 銀行切換後的 ROM 資料
    pub fn read_rom(&self, address: u16, rom: &[u8]) -> u8 {
        if address < 0x4000 {
            // Bank 0 固定
            if address as usize >= rom.len() {
                0xFF
            } else {
                rom[address as usize]
            }
        } else if address < 0x8000 {
            // Bank N
            let bank = self.rom_bank;
            let offset = (bank as usize) * 0x4000 + (address as usize - 0x4000);
            if offset >= rom.len() {
                0xFF
            } else {
                rom[offset]
            }
        } else {
            0xFF
        }
    }

    /// 寫入 MBC5 銀行切換控制暫存器
    pub fn write_rom(&mut self, address: u16, value: u8) {
        match address {
            0x0000..=0x1FFF => {
                // RAM enable
                self.ram_enabled = (value & 0x0F) == 0x0A;
            }
            0x2000..=0x2FFF => {
                // ROM bank low 8 bits
                self.rom_bank = (self.rom_bank & 0x100) | value as u16;
                if self.rom_bank == 0 {
                    self.rom_bank = 1;
                }
            }
            0x3000..=0x3FFF => {
                // ROM bank high bit (bit 8)
                self.rom_bank = (self.rom_bank & 0xFF) | (((value as u16) & 0x01) << 8);
                if self.rom_bank == 0 {
                    self.rom_bank = 1;
                }
            }
            0x4000..=0x5FFF => {
                // RAM bank
                self.ram_bank = value & 0x0F;
            }
            _ => {}
        }
    }

    /// 讀取 MBC5 SRAM
    pub fn read_ram(&self, address: u16) -> u8 {
        if !self.ram_enabled {
            return 0xFF;
        }
        let ram_addr = (self.ram_bank as usize) * 0x2000 + (address as usize - 0xA000);
        if ram_addr < self.ram.len() {
            self.ram[ram_addr]
        } else {
            0xFF
        }
    }

    /// 寫入 MBC5 SRAM
    pub fn write_ram(&mut self, address: u16, value: u8) {
        if !self.ram_enabled {
            return;
        }
        let ram_addr = (self.ram_bank as usize) * 0x2000 + (address as usize - 0xA000);
        if ram_addr < self.ram.len() {
            self.ram[ram_addr] = value;
        }
    }

    /// 處理寫入 MBC5 控制暫存器
    pub fn write(&mut self, address: u16, value: u8) {
        match address {
            0x0000..=0x1FFF => {
                // RAM enable
                self.ram_enabled = (value & 0x0F) == 0x0A;
            }
            0x2000..=0x2FFF => {
                // ROM bank low 8 bits
                self.rom_bank = (self.rom_bank & 0x100) | value as u16;
            }
            0x3000..=0x3FFF => {
                // ROM bank high bit
                self.rom_bank = (self.rom_bank & 0xFF) | (((value as u16) & 0x01) << 8);
            }
            0x4000..=0x5FFF => {
                // RAM bank
                self.ram_bank = value & 0x0F;
            }
            _ => {}
        }
    }

    /// 取得目前 ROM bank
    pub fn get_rom_bank(&self) -> u16 {
        self.rom_bank
    }

    /// 取得目前 RAM bank
    pub fn get_ram_bank(&self) -> u8 {
        self.ram_bank
    }

    /// 是否啟用 RAM
    pub fn is_ram_enabled(&self) -> bool {
        self.ram_enabled
    }
}
