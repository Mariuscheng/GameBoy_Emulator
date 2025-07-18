use crate::core::error::{Error, HardwareError, Result};
use crate::interface::input::joypad::Joypad;
use std::fs::OpenOptions;
use std::io::Write;

pub mod lcd_registers;
pub mod mbc;

use lcd_registers::LCDRegisters;

/// Nintendo Logo used for ROM validation
#[allow(dead_code)]
const NINTENDO_LOGO: &[u8; 48] = &[
    0xCE, 0xED, 0x66, 0x66, 0xCC, 0x0D, 0x00, 0x0B, 0x03, 0x73, 0x00, 0x83, 0x00, 0x0C, 0x00, 0x0D,
    0x00, 0x08, 0x11, 0x1F, 0x88, 0x89, 0x00, 0x0E, 0xDC, 0xCC, 0x6E, 0xE6, 0xDD, 0xDD, 0xD9, 0x99,
    0xBB, 0xBB, 0x67, 0x63, 0x6E, 0x0E, 0xEC, 0xCC, 0xDD, 0xDC, 0x99, 0x9F, 0xBB, 0xB9, 0x33, 0x3E,
];

/// Game Boy Memory Management Unit (MMU)
#[derive(Debug, Clone)]
pub struct MMU {
    pub cartridge_rom: Vec<u8>,
    pub work_ram: [u8; 0x2000],              // 8KB work RAM
    pub high_ram: [u8; 0x80],                // 128 bytes high RAM
    pub video_ram: [u8; 0x2000],             // 8KB video RAM
    pub object_attribute_memory: [u8; 0xA0], // Sprite Attribute Table
    pub io_registers: [u8; 0x80],            // I/O registers
    pub interrupt_enable: u8,                // 0xFFFF
    pub interrupt_flags: u8,                 // 0xFF0F
    pub lcd_registers: LCDRegisters,
    pub ly: u8,                        // Current scanline
    pub lyc: u8,                       // LY Compare
    pub instance_id: u64,              // Used to identify different MMU instances
    pub external_ram: Option<Vec<u8>>, // 外部 RAM (卡帶 RAM)
}

impl MMU {
    /// 每 frame 呼叫一次，模擬 timer 遞增與 V-Blank 中斷
    #[allow(dead_code)]
    pub fn tick(&mut self) {
        // DIV (0xFF04) 8-bit timer，簡單每次呼叫都加 1
        let div = self.io_registers[0x04].wrapping_add(1);
        self.io_registers[0x04] = div;

        // TIMA (0xFF05) 計時器，TAC (0xFF07) 啟用時才遞增
        let tac = self.io_registers[0x07];
        let timer_enable = tac & 0x04 != 0;
        if timer_enable {
            let tima = self.io_registers[0x05].wrapping_add(1);
            if tima == 0 {
                // 溢位，reload TMA 並觸發 timer interrupt (IF bit 2)
                self.io_registers[0x05] = self.io_registers[0x06];
                self.interrupt_flags |= 0x04;
            } else {
                self.io_registers[0x05] = tima;
            }
        }

        // 每 frame 觸發 V-Blank 中斷 (IF bit 0)
        self.interrupt_flags |= 0x01;
    }
    pub fn new() -> Self {
        log::info!("MMU::new() - Initialization started");
        if let Ok(mut file) = std::fs::OpenOptions::new()
            .create(true)
            .append(true)
            .open("logs/mmu_init.log")
        {
            let _ = writeln!(file, "[INFO] MMU::new() - Initialization started");
        }

        let instance_id = std::time::SystemTime::now()
            .duration_since(std::time::UNIX_EPOCH)
            .unwrap_or_default()
            .as_nanos() as u64;

        let mmu = Self {
            cartridge_rom: Vec::new(),
            work_ram: [0; 0x2000],
            high_ram: [0; 0x80],
            video_ram: [0; 0x2000],
            object_attribute_memory: [0; 0xA0],
            io_registers: [0; 0x80],
            interrupt_enable: 0,
            interrupt_flags: 0,
            lcd_registers: LCDRegisters::new(),
            ly: 0,
            lyc: 0,
            instance_id,
            external_ram: None,
        };

        log::info!("MMU::new() - Initialization completed");
        mmu
    }

    /// Debug VRAM access
    fn log_vram_access(&self, addr: u16, value: u8, is_write: bool) {
        if (0x8000..=0x9FFF).contains(&addr) {
            log::debug!(
                "VRAM {} at 0x{:04X} = 0x{:02X}",
                if is_write { "write" } else { "read" },
                addr,
                value
            );
        }
    }

    pub fn read_byte(&self, addr: u16) -> Result<u8> {
        let value = match addr {
            // ROM區域 (0x0000-0x7FFF)
            0x0000..=0x7FFF => {
                let index = addr as usize;
                if index >= self.cartridge_rom.len() {
                    log::warn!(
                        "ROM讀取越界: addr=0x{:04X}, rom_len=0x{:04X}",
                        addr,
                        self.cartridge_rom.len()
                    );
                    return Ok(0xFF);
                }

                let value = self.cartridge_rom[index];

                // 記錄所有ROM讀取
                if let Ok(mut file) = OpenOptions::new()
                    .create(true)
                    .append(true)
                    .open("logs/rom_read.log")
                {
                    let _ = writeln!(
                        file,
                        "ROM讀取 - 地址: 0x{:04X}, 值: 0x{:02X}, cartridge_len: 0x{:04X}",
                        addr,
                        value,
                        self.cartridge_rom.len()
                    );
                }

                value
            }

            // 視頻RAM (0x8000-0x9FFF)
            0x8000..=0x9FFF => {
                let value = self.video_ram[(addr - 0x8000) as usize];
                self.log_vram_access(addr, value, false);
                value
            }

            // 外部RAM (0xA000-0xBFFF)
            0xA000..=0xBFFF => {
                if let Some(ref ram) = self.external_ram {
                    let index = (addr - 0xA000) as usize;
                    if index < ram.len() {
                        ram[index]
                    } else {
                        log::warn!("訪問外部RAM越界: 0x{:04X}", addr);
                        0xFF
                    }
                } else {
                    log::debug!("讀取未初始化的外部RAM: 0x{:04X}", addr);
                    0xFF
                }
            }

            // 工作RAM (0xC000-0xDFFF)
            0xC000..=0xDFFF => self.work_ram[(addr - 0xC000) as usize],

            // 工作RAM鏡像 (0xE000-0xFDFF)
            0xE000..=0xFDFF => {
                let value = self.work_ram[(addr - 0xE000) as usize];
                log::debug!("讀取RAM鏡像: 0x{:04X} -> 0x{:02X}", addr, value);
                value
            }

            // OAM (0xFE00-0xFE9F)
            0xFE00..=0xFE9F => self.object_attribute_memory[(addr - 0xFE00) as usize],

            // 未使用區域 (0xFEA0-0xFEFF)
            0xFEA0..=0xFEFF => 0xFF,

            // I/O寄存器 (0xFF00-0xFF7F)
            0xFF00..=0xFF7F => match addr {
                0xFF0F => self.interrupt_flags,
                _ => self.io_registers[(addr - 0xFF00) as usize],
            },

            // 高RAM (0xFF80-0xFFFE)
            0xFF80..=0xFFFE => self.high_ram[(addr - 0xFF80) as usize],

            // 中斷使能寄存器 (0xFFFF)
            0xFFFF => self.interrupt_enable,
        };

        // 記錄可疑區域的訪問
        if (0xCDC3..=0xCDFF).contains(&addr) {
            log::warn!("從可疑區域讀取: addr=0x{:04X}, value=0x{:02X}", addr, value);
            if let Ok(mut file) = OpenOptions::new()
                .create(true)
                .append(true)
                .open("logs/suspicious_reads.log")
            {
                writeln!(file, "可疑讀取: addr=0x{:04X}, value=0x{:02X}", addr, value).ok();
            }
        }

        Ok(value)
    }

    pub fn write_byte(&mut self, addr: u16, value: u8) -> Result<()> {
        match addr {
            // ROM 區域 (0x0000-0x7FFF)
            0x0000..=0x7FFF => {
                // 記錄嘗試寫入ROM的行為，但不阻止它
                log::warn!(
                    "嘗試寫入唯讀記憶體: addr=0x{:04X}, value=0x{:02X}",
                    addr,
                    value
                );
                Ok(()) // 允許寫入，但不實際修改數據
            }

            // 可寫入的 RAM 區域
            0x8000..=0x9FFF => {
                let offset = addr as usize - 0x8000;
                self.video_ram[offset] = value;
                self.log_vram_access(addr, value, true);
                Ok(())
            }

            // 外部 RAM (卡帶 RAM)
            0xA000..=0xBFFF => {
                if let Some(ref mut ram) = self.external_ram {
                    let offset = (addr as usize - 0xA000) % ram.len();
                    ram[offset] = value;
                    Ok(())
                } else {
                    log::warn!(
                        "嘗試寫入未啟用的外部RAM: addr=0x{:04X}, value=0x{:02X}",
                        addr,
                        value
                    );
                    Ok(())
                }
            }

            // 工作 RAM
            0xC000..=0xDFFF => {
                let offset = addr as usize - 0xC000;
                self.work_ram[offset] = value;
                Ok(())
            }

            // Echo RAM (工作RAM的鏡像)
            0xE000..=0xFDFF => {
                let offset = addr as usize - 0xE000;
                self.work_ram[offset] = value;
                Ok(())
            }

            // Sprite Attribute Table (OAM)
            0xFE00..=0xFE9F => {
                let offset = addr as usize - 0xFE00;
                self.object_attribute_memory[offset] = value;
                Ok(())
            }

            // 保留區域
            0xFEA0..=0xFEFF => {
                log::warn!("嘗試寫入保留記憶體區域: addr=0x{:04X}", addr);
                Ok(())
            }

            // I/O 寄存器
            0xFF00..=0xFF7F => {
                let offset = addr as usize - 0xFF00;
                self.io_registers[offset] = value;
                Ok(())
            }

            // High RAM
            0xFF80..=0xFFFE => {
                let offset = addr as usize - 0xFF80;
                self.high_ram[offset] = value;
                Ok(())
            }

            // 中斷啟用寄存器
            0xFFFF => {
                self.interrupt_enable = value;
                Ok(())
            }
        }
    }

    /// 寫入一個 16-bit 值到記憶體
    pub fn write_word(&mut self, addr: u16, value: u16) -> Result<()> {
        let low = (value & 0xFF) as u8;
        let high = ((value >> 8) & 0xFF) as u8;
        self.write_byte(addr, low)?;
        self.write_byte(addr.wrapping_add(1), high)?;
        Ok(())
    }

    /// Execute DMA transfer (0xFF46)
    #[allow(dead_code)]
    fn dma_transfer(&mut self, value: u8) -> Result<()> {
        // DMA transfer starts from address (value << 8)
        let source_addr = (value as u16) << 8;
        log::info!("Starting DMA transfer from 0x{:04X}", source_addr);

        // Transfer 160 bytes to OAM
        for i in 0..0xA0 {
            let byte = self.read_byte(source_addr + i)?;
            self.object_attribute_memory[i as usize] = byte;
        }

        log::info!("DMA transfer completed");
        Ok(())
    }

    /// Write to I/O registers
    #[allow(dead_code)]
    fn write_io_register(&mut self, address: u16, value: u8) -> Result<()> {
        match address {
            0xFF46 => self.dma_transfer(value)?, // DMA transfer
            0xFF40..=0xFF4B => self.write_lcd_register(address, value)?,
            0xFF80..=0xFFFE => self.high_ram[(address - 0xFF80) as usize] = value,
            0xFFFF => self.interrupt_enable = value,
            _ => self.io_registers[(address - 0xFF00) as usize] = value,
        }
        Ok(())
    }

    #[allow(dead_code)]
    fn write_lcd_register(&mut self, address: u16, value: u8) -> Result<()> {
        match address {
            0xFF40 => {
                self.lcd_registers.lcdc = value;
                Ok(())
            }
            0xFF41 => {
                self.lcd_registers.stat = value;
                Ok(())
            }
            0xFF42 => {
                self.lcd_registers.scy = value;
                Ok(())
            }
            0xFF43 => {
                self.lcd_registers.scx = value;
                Ok(())
            }
            0xFF44 => Ok(()), // LY is read-only
            0xFF45 => {
                self.lyc = value;
                Ok(())
            }
            0xFF47 => {
                self.lcd_registers.bgp = value;
                Ok(())
            }
            0xFF48 => {
                self.lcd_registers.obp0 = value;
                Ok(())
            }
            0xFF49 => {
                self.lcd_registers.obp1 = value;
                Ok(())
            }
            0xFF4A => {
                self.lcd_registers.wy = value;
                Ok(())
            }
            0xFF4B => {
                self.lcd_registers.wx = value;
                Ok(())
            }
            _ => {
                self.io_registers[(address - 0xFF00) as usize] = value;
                Ok(())
            }
        }
    }

    pub fn load_rom(&mut self, rom_data: Vec<u8>) -> Result<()> {
        // 記錄ROM載入
        let mut file = OpenOptions::new()
            .create(true)
            .append(true)
            .open("logs/rom_load.log")?;

        writeln!(file, "開始載入ROM...")?;
        writeln!(file, "ROM大小: {} bytes", rom_data.len())?;

        // 記錄ROM的前32字節
        writeln!(file, "\nROM前32字節:")?;
        for i in 0..32.min(rom_data.len()) {
            write!(file, "{:02X} ", rom_data[i])?;
            if (i + 1) % 16 == 0 {
                writeln!(file)?;
            }
        }
        writeln!(file)?;

        // 驗證ROM大小
        if rom_data.len() < 0x150 {
            writeln!(file, "ROM大小驗證失敗: {} bytes", rom_data.len())?;
            return Err(Error::Hardware(HardwareError::InvalidROMSize));
        }

        // 檢查Nintendo標誌
        for (i, &byte) in NINTENDO_LOGO.iter().enumerate() {
            if rom_data[0x104 + i] != byte {
                writeln!(file, "Nintendo標誌驗證失敗於位置 0x{:04X}", 0x104 + i)?;
                return Err(Error::Hardware(HardwareError::InvalidROM));
            }
        }

        // 驗證ROM檢查和
        let mut checksum: u8 = 0;
        for &byte in rom_data[0x134..=0x14C].iter() {
            checksum = checksum.wrapping_sub(byte).wrapping_sub(1);
        }
        if checksum != rom_data[0x14D] {
            log::error!("ROM檢查和驗證失敗");
            return Err(Error::Hardware(HardwareError::InvalidROM));
        }

        writeln!(file, "ROM驗證成功")?;

        // 計算ROM大小
        let rom_size = match rom_data[0x148] {
            0x00 => 32768,  // 32KB (無MBC)
            0x01 => 65536,  // 64KB
            0x02 => 131072, // 128KB
            0x03 => 262144, // 256KB
            0x04 => 524288, // 512KB
            _ => {
                writeln!(
                    file,
                    "未知的ROM大小代碼: 0x{:02X}, 使用實際大小",
                    rom_data[0x148]
                )?;
                rom_data.len()
            }
        };

        // 儲存ROM數據之前先檢查內容
        writeln!(file, "\n檢查ROM數據的關鍵區域:")?;
        writeln!(file, "引導區域 (0x0000-0x00FF):")?;
        for i in 0..16 {
            write!(file, "{:02X} ", rom_data[i])?;
        }
        writeln!(file)?;

        writeln!(file, "標頭區域 (0x0134-0x0143):")?;
        for i in 0x134..0x144 {
            write!(file, "{:02X} ", rom_data[i])?;
        }
        writeln!(file)?;

        // 儲存ROM數據
        self.cartridge_rom = rom_data.clone(); // 使用clone以確保完整複製

        // 驗證儲存後的數據
        writeln!(file, "\n驗證MMU中的ROM數據:")?;
        writeln!(file, "MMU中的引導區域 (0x0000-0x00FF):")?;
        for i in 0..16 {
            write!(file, "{:02X} ", self.cartridge_rom[i])?;
        }
        writeln!(file)?;

        // 初始化VRAM
        writeln!(file, "\n初始化VRAM...")?;
        self.video_ram.fill(0);

        // 記錄VRAM數據載入狀態
        writeln!(file, "VRAM載入狀態:")?;
        let non_zero = self.video_ram.iter().filter(|&&x| x != 0).count();
        writeln!(file, "非零數據數量: {}", non_zero)?;
        writeln!(file, "VRAM前32字節:")?;
        for i in 0..32 {
            write!(file, "{:02X} ", self.video_ram[i])?;
            if (i + 1) % 16 == 0 {
                writeln!(file)?;
            }
        }
        writeln!(file)?; // 設定基礎調色盤 (與DMG-ACID2相容)
        self.io_registers[0x47] = 0xE4; // BGP - 背景調色盤 (0xFF47 - 0xFF00 = 0x47)

        writeln!(file, "VRAM初始化完成")?;
        writeln!(file, "VRAM數據示例:")?;
        for i in 0..16 {
            write!(file, "{:02X} ", self.video_ram[i])?;
            if (i + 1) % 8 == 0 {
                writeln!(file)?;
            }
        }

        // 初始化外部RAM（如果需要）
        let ram_size = match self.cartridge_rom[0x149] {
            0x00 => 0,     // 無RAM
            0x01 => 2048,  // 2KB
            0x02 => 8192,  // 8KB
            0x03 => 32768, // 32KB
            _ => 0,
        };

        if ram_size > 0 {
            self.external_ram = Some(vec![0; ram_size]);
        }

        log::info!(
            "載入ROM成功：大小={} bytes, 類型=0x{:02X}, RAM大小={} bytes",
            rom_size,
            self.cartridge_rom[0x147],
            ram_size
        );

        Ok(())
    }
    pub fn reset(&mut self) {
        // 清空所有內存區域
        self.work_ram.fill(0);
        self.high_ram.fill(0);
        self.video_ram.fill(0);
        self.object_attribute_memory.fill(0);
        self.io_registers.fill(0);

        // 重置中斷狀態
        self.interrupt_enable = 0;
        self.interrupt_flags = 0;

        // 初始化 LCD 寄存器
        self.lcd_registers = LCDRegisters::new();

        // 重置掃描線相關寄存器
        self.ly = 0; // 當前掃描線
        self.lyc = 0; // 掃描線比較值

        log::info!("MMU reset completed");
        log::info!(
            "LCD registers reset: LCDC={:02X}h, STAT={:02X}h, BGP={:02X}h",
            self.lcd_registers.lcdc,
            self.lcd_registers.stat,
            self.lcd_registers.bgp
        );
    }
    pub fn update_joypad_state(&mut self, joypad: &dyn Joypad) {
        let mut value = 0xFF;

        // Select button state if requested
        if (self.io_registers[0x00] & 0x20) == 0 {
            value &= !(((joypad.is_start_pressed() as u8) << 3)
                | ((joypad.is_select_pressed() as u8) << 2)
                | ((joypad.is_b_pressed() as u8) << 1)
                | (joypad.is_a_pressed() as u8));
        }

        // Select directional state if requested
        if (self.io_registers[0x00] & 0x10) == 0 {
            value &= !(((joypad.is_down_pressed() as u8) << 3)
                | ((joypad.is_up_pressed() as u8) << 2)
                | ((joypad.is_left_pressed() as u8) << 1)
                | (joypad.is_right_pressed() as u8));
        }

        self.io_registers[0x00] = (self.io_registers[0x00] & 0xF0) | (value & 0x0F);
    }
}

impl Default for MMU {
    fn default() -> Self {
        Self::new()
    }
}
