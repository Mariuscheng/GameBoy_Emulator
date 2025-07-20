//! Game Boy ROM header inspection tool
use std::fs;

/// 檢查並回傳 ROM entry 資訊
pub struct RomEntry {
    pub title: String,
    pub cgb_flag: u8,
    pub licensee: u8,
    pub sgb_flag: u8,
    pub cart_type: u8,
    pub rom_size: u8,
    pub ram_size: u8,
    pub checksum: u8,
    pub global_checksum: u16,
}

pub fn get_rom_entry(path: &str) -> Option<RomEntry> {
    let data = match fs::read(path) {
        Ok(d) => d,
        Err(_) => return None,
    };
    if data.len() < 0x150 {
        return None;
    }
    let title = String::from_utf8_lossy(&data[0x134..0x144])
        .trim()
        .to_string();
    Some(RomEntry {
        title,
        cgb_flag: data[0x143],
        licensee: data[0x144],
        sgb_flag: data[0x146],
        cart_type: data[0x147],
        rom_size: data[0x148],
        ram_size: data[0x149],
        checksum: data[0x14D],
        global_checksum: ((data[0x14E] as u16) << 8) | data[0x14F] as u16,
    })
}

// 保留原本的 check_rom_header 方便 CLI debug
pub fn check_rom_header(path: &str) {
    if let Some(entry) = get_rom_entry(path) {
        println!("ROM Title: {}", entry.title);
        println!("CGB Flag: 0x{:02X}", entry.cgb_flag);
        println!("Licensee: 0x{:02X}", entry.licensee);
        println!("SGB Flag: 0x{:02X}", entry.sgb_flag);
        println!("Cartridge Type: 0x{:02X}", entry.cart_type);
        println!("ROM Size: 0x{:02X}", entry.rom_size);
        println!("RAM Size: 0x{:02X}", entry.ram_size);
        println!("Header Checksum: 0x{:02X}", entry.checksum);
        println!("Global Checksum: 0x{:04X}", entry.global_checksum);
    } else {
        println!("❌ Failed to read or parse ROM entry");
    }
}
