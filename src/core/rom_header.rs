//! Game Boy ROM Header 結構與解析工具

#[derive(Debug, Clone)]
pub struct RomHeader {
    pub title: String,
    pub cgb_flag: u8,
    pub licensee: u8,
    pub sgb_flag: u8,
    pub cartridge_type: u8,
    pub rom_size: u8,
    pub ram_size: u8,
    pub header_checksum: u8,
    pub global_checksum: u16,
}

impl RomHeader {
    /// 解析 ROM header
    pub fn parse(data: &[u8]) -> Option<Self> {
        if data.len() < 0x150 {
            return None;
        }
        Some(Self {
            title: String::from_utf8_lossy(&data[0x134..0x144])
                .trim_end_matches('\0')
                .to_string(),
            cgb_flag: data[0x143],
            licensee: data[0x144],
            sgb_flag: data[0x146],
            cartridge_type: data[0x147],
            rom_size: data[0x148],
            ram_size: data[0x149],
            header_checksum: data[0x14D],
            global_checksum: u16::from_be_bytes([data[0x14E], data[0x14F]]),
        })
    }

    /// 驗證 header checksum
    pub fn validate_checksum(&self, data: &[u8]) -> bool {
        let mut sum: u8 = 0;
        for i in 0x134..=0x14C {
            sum = sum.wrapping_sub(data[i]).wrapping_sub(1);
        }
        sum == self.header_checksum
    }
}

// 可擴充 cartridge type/size 對應表、licensee 對應表等

/// 範例：顯示指定 ROM 路徑的 header 資訊
pub fn display_rom_header_info() {
    let path = "roms/rom.gb";
    let data = std::fs::read(path).expect("無法讀取 ROM 檔案");
    if let Some(header) = RomHeader::parse(&data) {
        println!("ROM Title: {}", header.title);
        println!("CGB Flag: {:02X}", header.cgb_flag);
        println!("Licensee: {:02X}", header.licensee);
        println!("SGB Flag: {:02X}", header.sgb_flag);
        println!("Cartridge Type: {:02X}", header.cartridge_type);
        println!("ROM Size: {:02X}", header.rom_size);
        println!("RAM Size: {:02X}", header.ram_size);
        println!("Header Checksum: {:02X}", header.header_checksum);
        println!("Global Checksum: {:04X}", header.global_checksum);
        println!("Checksum Valid: {}", header.validate_checksum(&data));
    } else {
        println!("ROM header 解析失敗");
    }
}
