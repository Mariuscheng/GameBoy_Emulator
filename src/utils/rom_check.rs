//! Game Boy ROM header inspection tool
use std::fs;

pub fn check_rom_header(path: &str) {
    let data = match fs::read(path) {
        Ok(d) => d,
        Err(e) => {
            log::error!("❌ Failed to read ROM: {}", e);
            return;
        }
    };
    if data.len() < 0x150 {
        log::error!("❌ ROM file too small: {} bytes", data.len());
        return;
    }
    let title = &data[0x134..0x144];
    let title_str = String::from_utf8_lossy(title);
    let cgb_flag = data[0x143];
    let licensee = data[0x144];
    let sgb_flag = data[0x146];
    let cart_type = data[0x147];
    let rom_size = data[0x148];
    let ram_size = data[0x149];
    let checksum = data[0x14D];
    let global_checksum = ((data[0x14E] as u16) << 8) | data[0x14F] as u16;
    log::info!("ROM Title: {}", title_str.trim());
    log::info!("CGB Flag: 0x{:02X}", cgb_flag);
    log::info!("Licensee: 0x{:02X}", licensee);
    log::info!("SGB Flag: 0x{:02X}", sgb_flag);
    log::info!("Cartridge Type: 0x{:02X}", cart_type);
    log::info!("ROM Size: 0x{:02X}", rom_size);
    log::info!("RAM Size: 0x{:02X}", ram_size);
    log::info!("Header Checksum: 0x{:02X}", checksum);
    log::info!("Global Checksum: 0x{:04X}", global_checksum);
}
