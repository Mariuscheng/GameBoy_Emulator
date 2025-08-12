use crate::GB::RAM::RAM;

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
enum MbcType {
    None,
    Mbc1,
    Mbc3,
    Mbc5,
}

// Minimal Bus/MMU abstraction that proxies to RAM and maps key I/O registers.
pub struct Bus {
    ram: RAM,
    // Cartridge ROM and MBC
    rom: Vec<u8>,
    rom_banks: usize, // number of 16KB banks
    mbc: MbcType,
    // Common MBC state
    ram_enable: bool,
    rom_bank: u16,    // current switchable ROM bank (for 0x4000..0x7FFF)
    ram_bank: u8,     // current external RAM bank
    ext_ram: Vec<u8>, // external RAM backing store (multiple 8KB banks)
    // MBC1-specific
    mbc1_bank_low5: u8,
    mbc1_bank_high2: u8,
    mbc1_mode: u8, // 0=ROM banking, 1=RAM banking
    // MBC3-specific (basic RTC stub)
    mbc3_rtc_sel: Option<u8>, // 0x08..0x0C when selecting RTC regs
    mbc3_rtc_regs: [u8; 5],   // S, M, H, DL, DH
    // Interrupt registers
    ie: u8,  // 0xFFFF
    ifl: u8, // 0xFF0F
    // Joypad
    p1: u8, // 0xFF00
    // Timer
    div: u8,  // 0xFF04
    tima: u8, // 0xFF05
    tma: u8,  // 0xFF06
    tac: u8,  // 0xFF07
    // DMA
    dma: u8, // 0xFF46 (last written value)
    // PPU basic registers
    lcdc: u8,   // 0xFF40
    stat_w: u8, // writable bits of STAT (we keep bits 3..6); read composes mode & coincidence
    scy: u8,    // 0xFF42
    scx: u8,    // 0xFF43
    ly: u8,     // 0xFF44 (read-only; write resets to 0)
    lyc: u8,    // 0xFF45
    bgp: u8,    // 0xFF47
    obp0: u8,   // 0xFF48
    obp1: u8,   // 0xFF49
    wy: u8,     // 0xFF4A
    wx: u8,     // 0xFF4B
    // PPU timing
    ppu_line_cycle: u32, // 0..=455 within a scanline
    ppu_mode: u8,        // 0=HBlank,1=VBlank,2=OAM,3=Transfer
    // Simple framebuffer (DMG shades 0..3)
    framebuffer: [u8; 160 * 144],
    // internal counters
    div_counter: u32,
    tima_counter: u32,
    // debug once-only markers
    dbg_lcdc_first_write_done: bool,
    dbg_vram_first_write_done: bool,
}

impl Bus {
    pub fn new() -> Self {
        Self {
            ram: RAM::new(),
            rom: Vec::new(),
            rom_banks: 0,
            mbc: MbcType::None,
            ram_enable: false,
            rom_bank: 1,
            ram_bank: 0,
            ext_ram: Vec::new(),
            mbc1_bank_low5: 1,
            mbc1_bank_high2: 0,
            mbc1_mode: 0,
            mbc3_rtc_sel: None,
            mbc3_rtc_regs: [0; 5],
            ie: 0x00,
            ifl: 0x00,
            p1: 0xFF, // default: all inputs high, no selection
            div: 0x00,
            tima: 0x00,
            tma: 0x00,
            tac: 0x00,
            dma: 0x00,
            lcdc: 0x00,
            stat_w: 0x00,
            scy: 0x00,
            scx: 0x00,
            ly: 0x00,
            lyc: 0x00,
            bgp: 0xFC, // typical DMG default
            obp0: 0xFF,
            obp1: 0xFF,
            wy: 0x00,
            wx: 0x00,
            ppu_line_cycle: 0,
            ppu_mode: 2, // power-on: assume start of a line in OAM search
            framebuffer: [0; 160 * 144],
            div_counter: 0,
            tima_counter: 0,
            dbg_lcdc_first_write_done: false,
            dbg_vram_first_write_done: false,
        }
    }

    /// Load a full ROM image and initialize basic MBC state
    pub fn load_rom(&mut self, data: Vec<u8>) {
        self.rom = data;
        self.rom_banks = (self.rom.len() + 0x3FFF) / 0x4000; // ceil
        // Detect cartridge type from header 0x0147 (if present)
        let cart_type = if self.rom.len() > 0x0147 {
            self.rom[0x0147]
        } else {
            0x00
        };
        self.mbc = match cart_type {
            0x01 | 0x02 | 0x03 => MbcType::Mbc1,
            0x0F | 0x10 | 0x11 | 0x12 | 0x13 => MbcType::Mbc3, // MBC3 (+Timer,Battery,RAM)
            0x19 | 0x1A | 0x1B | 0x1C | 0x1D | 0x1E => MbcType::Mbc5,
            _ => MbcType::None,
        };
        // External RAM size (header 0x0149)
        let ram_size_code = if self.rom.len() > 0x0149 {
            self.rom[0x0149]
        } else {
            0
        };
        let ram_banks = match ram_size_code {
            0x02 => 1,  // 8KB
            0x03 => 4,  // 32KB
            0x04 => 16, // 128KB
            0x05 => 8,  // 64KB
            _ => 0,
        };
        self.ext_ram = vec![0u8; ram_banks * 0x2000];
        // Reset MBC state
        self.ram_enable = false;
        self.rom_bank = 1;
        self.ram_bank = 0;
        self.mbc1_bank_low5 = 1;
        self.mbc1_bank_high2 = 0;
        self.mbc1_mode = 0;
        self.mbc3_rtc_sel = None;
        self.mbc3_rtc_regs = [0; 5];
        // Note: We no longer mirror ROM into RAM for 0x0000..0x7FFF; reads go via self.rom
        println!(
            "[Bus] ROM loaded: {} bytes, banks={}, MBC={:?}, extRAM={} bytes",
            self.rom.len(),
            self.rom_banks,
            self.mbc,
            self.ext_ram.len()
        );
    }

    #[inline]
    fn mbc1_calc_bank0(&self) -> u16 {
        if self.mbc1_mode & 1 == 0 {
            0
        } else {
            ((self.mbc1_bank_high2 as u16) & 0x03) << 5
        }
    }
    #[inline]
    fn mbc1_calc_bankX(&self) -> u16 {
        let low5 = (self.mbc1_bank_low5 as u16) & 0x1F;
        let mut bank = if self.mbc1_mode & 1 == 0 {
            // ROM banking mode: combine high2:low5
            (low5) | (((self.mbc1_bank_high2 as u16) & 0x03) << 5)
        } else {
            // RAM banking mode: only low5 applies to ROM bank
            low5
        };
        if (bank & 0x1F) == 0 {
            bank |= 1;
        }
        bank
    }
    #[inline]
    fn read_rom(&self, addr: u16) -> u8 {
        if self.rom_banks == 0 {
            return 0xFF;
        }
        match self.mbc {
            MbcType::None => {
                let i = addr as usize;
                if i < self.rom.len() {
                    self.rom[i]
                } else {
                    0xFF
                }
            }
            MbcType::Mbc1 => {
                if addr < 0x4000 {
                    let bank0 = (self.mbc1_calc_bank0() as usize) % self.rom_banks;
                    let base = bank0 * 0x4000;
                    let i = base + addr as usize;
                    if i < self.rom.len() {
                        self.rom[i]
                    } else {
                        0xFF
                    }
                } else {
                    let bankx = (self.mbc1_calc_bankX() as usize) % self.rom_banks;
                    let base = bankx * 0x4000;
                    let i = base + (addr as usize - 0x4000);
                    if i < self.rom.len() {
                        self.rom[i]
                    } else {
                        0xFF
                    }
                }
            }
            MbcType::Mbc3 => {
                if addr < 0x4000 {
                    let i = addr as usize;
                    if i < self.rom.len() {
                        self.rom[i]
                    } else {
                        0xFF
                    }
                } else {
                    let mut bank = (self.rom_bank as usize) & 0x7F; // 7-bit
                    if bank == 0 {
                        bank = 1;
                    }
                    let base = (bank % self.rom_banks) * 0x4000;
                    let i = base + (addr as usize - 0x4000);
                    if i < self.rom.len() {
                        self.rom[i]
                    } else {
                        0xFF
                    }
                }
            }
            MbcType::Mbc5 => {
                if addr < 0x4000 {
                    let base = 0usize; // fixed bank 0
                    let i = base + addr as usize;
                    if i < self.rom.len() {
                        self.rom[i]
                    } else {
                        0xFF
                    }
                } else {
                    let bank = (self.rom_bank as usize) % self.rom_banks;
                    let base = bank * 0x4000;
                    let i = base + (addr as usize - 0x4000);
                    if i < self.rom.len() {
                        self.rom[i]
                    } else {
                        0xFF
                    }
                }
            }
        }
    }

    #[inline]
    fn write_mbc(&mut self, addr: u16, val: u8) {
        match self.mbc {
            MbcType::None => { /* ignore writes to 0x0000..0x7FFF */ }
            MbcType::Mbc1 => {
                match addr {
                    0x0000..=0x1FFF => {
                        self.ram_enable = (val & 0x0F) == 0x0A;
                    }
                    0x2000..=0x3FFF => {
                        let mut low5 = val & 0x1F;
                        if low5 == 0 {
                            low5 = 1;
                        }
                        self.mbc1_bank_low5 = low5;
                    }
                    0x4000..=0x5FFF => {
                        self.mbc1_bank_high2 = val & 0x03;
                        if self.mbc1_mode & 1 == 1 {
                            // RAM bank is high2 in RAM banking mode
                            self.ram_bank = self.mbc1_bank_high2 & 0x03;
                        }
                    }
                    0x6000..=0x7FFF => {
                        self.mbc1_mode = val & 0x01;
                    }
                    _ => {}
                }
            }
            MbcType::Mbc3 => {
                match addr {
                    0x0000..=0x1FFF => {
                        // RAM enable
                        self.ram_enable = (val & 0x0F) == 0x0A;
                    }
                    0x2000..=0x3FFF => {
                        // ROM bank (7-bit), treat 0 as 1
                        let mut b = (val & 0x7F) as u16;
                        if b == 0 {
                            b = 1;
                        }
                        self.rom_bank = b;
                    }
                    0x4000..=0x5FFF => {
                        // RAM bank (0..3) or RTC select (0x08..0x0C)
                        let v = val & 0x0F;
                        if v <= 0x03 {
                            self.ram_bank = v;
                            self.mbc3_rtc_sel = None;
                        } else if (0x08..=0x0C).contains(&v) {
                            self.mbc3_rtc_sel = Some(v);
                        }
                    }
                    0x6000..=0x7FFF => {
                        // Latch clock: usually write 0 then 1; we ignore and keep simple stub
                        let _ = val;
                    }
                    _ => {}
                }
            }
            MbcType::Mbc5 => {
                match addr {
                    0x0000..=0x1FFF => {
                        self.ram_enable = (val & 0x0F) == 0x0A;
                    }
                    0x2000..=0x2FFF => {
                        self.rom_bank = (self.rom_bank & 0x100) | (val as u16);
                    }
                    0x3000..=0x3FFF => {
                        self.rom_bank = (self.rom_bank & 0x0FF) | (((val as u16) & 0x01) << 8);
                    }
                    0x4000..=0x5FFF => {
                        self.ram_bank = val & 0x0F;
                    }
                    0x6000..=0x7FFF => {
                        // MBC5: rumble enable lives here for some carts; ignore
                    }
                    _ => {}
                }
            }
        }
    }

    #[inline]
    fn bg_tilemap_base(&self) -> u16 {
        if (self.lcdc & 0x08) != 0 {
            0x9C00
        } else {
            0x9800
        }
    }

    #[inline]
    fn bg_tiledata_signed(&self) -> bool {
        (self.lcdc & 0x10) == 0 // 0 -> 0x8800 signed, 1 -> 0x8000 unsigned
    }

    fn render_scanline(&mut self) {
        let y = self.ly as usize;
        if y >= 144 {
            return;
        }
        let mut bg_color_idx = [0u8; 160];
        let mut shades = [0u8; 160];

        // Background
        if (self.lcdc & 0x01) != 0 {
            let scy = self.scy as u16;
            let scx = self.scx as u16;
            let v = ((self.ly as u16).wrapping_add(scy)) & 0xFF;
            let tilemap = self.bg_tilemap_base();
            let signed = self.bg_tiledata_signed();
            let row_in_tile = (v & 7) as u16;
            let tile_row = (v >> 3) as u16;
            for x in 0..160u16 {
                let h = (x.wrapping_add(scx)) & 0xFF;
                let tile_col = (h >> 3) as u16;
                let map_index = tile_row * 32 + tile_col;
                let tile_id = self.ram.read(tilemap + map_index);
                let tile_addr = if signed {
                    let idx = tile_id as i8 as i16;
                    let base = 0x9000i32 + (idx as i32) * 16;
                    base as u16
                } else {
                    0x8000u16 + (tile_id as u16) * 16
                };
                let lo = self.ram.read(tile_addr + row_in_tile * 2);
                let hi = self.ram.read(tile_addr + row_in_tile * 2 + 1);
                let bit = 7 - ((h & 7) as u8);
                let lo_b = (lo >> bit) & 1;
                let hi_b = (hi >> bit) & 1;
                let color = (hi_b << 1) | lo_b;
                let shade = (self.bgp >> (color * 2)) & 0x03;
                bg_color_idx[x as usize] = color;
                shades[x as usize] = shade;
            }
        }

        // Window overlays BG
        if (self.lcdc & 0x20) != 0 && (self.lcdc & 0x01) != 0 {
            let wy = self.wy as u16;
            if (self.ly as u16) >= wy {
                let tilemap = if (self.lcdc & 0x40) != 0 {
                    0x9C00
                } else {
                    0x9800
                };
                let signed = self.bg_tiledata_signed();
                let wy_line = (self.ly as u16).wrapping_sub(wy);
                let row_in_tile = (wy_line & 7) as u16;
                let tile_row = (wy_line >> 3) as u16;
                let wx = self.wx as i16 - 7;
                for x in 0..160i16 {
                    if x < wx {
                        continue;
                    }
                    let wx_col = (x - wx) as u16;
                    let tile_col = (wx_col >> 3) as u16;
                    let map_index = tile_row * 32 + tile_col;
                    let tile_id = self.ram.read(tilemap + map_index);
                    let tile_addr = if signed {
                        let idx = tile_id as i8 as i16;
                        let base = 0x9000i32 + (idx as i32) * 16;
                        base as u16
                    } else {
                        0x8000u16 + (tile_id as u16) * 16
                    };
                    let lo = self.ram.read(tile_addr + row_in_tile * 2);
                    let hi = self.ram.read(tile_addr + row_in_tile * 2 + 1);
                    let bit = 7 - ((wx_col & 7) as u8);
                    let lo_b = (lo >> bit) & 1;
                    let hi_b = (hi >> bit) & 1;
                    let color = (hi_b << 1) | lo_b;
                    let shade = (self.bgp >> (color * 2)) & 0x03;
                    let xi = x as usize;
                    if xi < 160 {
                        bg_color_idx[xi] = color;
                        shades[xi] = shade;
                    }
                }
            }
        }

        // Sprites overlay
        if (self.lcdc & 0x02) != 0 {
            let obj_size_8x16 = (self.lcdc & 0x04) != 0;
            let mut sprite_written = [false; 160];
            for i in 0..40u16 {
                let oam = 0xFE00 + i * 4;
                let sy = self.ram.read(oam) as i16 - 16;
                let sx = self.ram.read(oam + 1) as i16 - 8;
                let mut tile = self.ram.read(oam + 2);
                let attr = self.ram.read(oam + 3);
                let palette = if (attr & 0x10) != 0 {
                    self.obp1
                } else {
                    self.obp0
                };
                let xflip = (attr & 0x20) != 0;
                let yflip = (attr & 0x40) != 0;
                let behind_bg = (attr & 0x80) != 0;
                let height = if obj_size_8x16 { 16 } else { 8 };
                let y = self.ly as i16;
                if y < sy || y >= sy + height {
                    continue;
                }
                let mut row = (y - sy) as u16;
                if yflip {
                    row = (height - 1) as u16 - row;
                }
                if obj_size_8x16 {
                    tile &= 0xFE;
                }
                let tile_addr = 0x8000u16 + (tile as u16) * 16 + (row as u16) * 2;
                let lo = self.ram.read(tile_addr);
                let hi = self.ram.read(tile_addr + 1);
                for px in 0..8u16 {
                    let mut bit = 7 - px as u8;
                    if xflip {
                        bit = px as u8;
                    }
                    let lo_b = (lo >> bit) & 1;
                    let hi_b = (hi >> bit) & 1;
                    let color = (hi_b << 1) | lo_b;
                    if color == 0 {
                        continue;
                    }
                    let x = sx + px as i16;
                    if x < 0 || x >= 160 {
                        continue;
                    }
                    let xi = x as usize;
                    if sprite_written[xi] {
                        continue;
                    }
                    if behind_bg && bg_color_idx[xi] != 0 {
                        continue;
                    }
                    let shade = (palette >> (color * 2)) & 0x03;
                    shades[xi] = shade;
                    sprite_written[xi] = true;
                }
            }
        }

        // Write out
        let base = y * 160;
        for x in 0..160usize {
            self.framebuffer[base + x] = shades[x];
        }
    }

    pub fn get_fb_pixel(&self, x: usize, y: usize) -> u8 {
        if x < 160 && y < 144 {
            self.framebuffer[y * 160 + x]
        } else {
            0
        }
    }

    #[inline]
    pub fn read(&self, addr: u16) -> u8 {
        match addr {
            0x0000..=0x7FFF => {
                // If a ROM is loaded, read via MBC mapping; otherwise fall back to RAM
                if self.rom_banks > 0 {
                    return self.read_rom(addr);
                } else {
                    return self.ram.read(addr);
                }
            }
            0xFF40 => self.lcdc,
            0xFF41 => {
                // Compose STAT: bit7=1; bits3..6 from stat_w; bit2 coincidence; bits1..0 current mode
                let coincidence = if self.ly == self.lyc { 0x04 } else { 0x00 };
                0x80 | (self.stat_w & 0x78) | coincidence | (self.ppu_mode & 0x03)
            }
            0xFF42 => self.scy,
            0xFF43 => self.scx,
            0xFF44 => self.ly,
            0xFF45 => self.lyc,
            0xFFFF => self.ie,
            0xFF0F => self.ifl | 0xE0, // upper bits often read as 1 on real HW; mask to be safe
            0xFF00 => self.p1,
            0xFF04 => self.div,
            0xFF05 => self.tima,
            0xFF06 => self.tma,
            0xFF07 => self.tac | 0xF8, // only low 3 bits are used
            0xFF47 => self.bgp,
            0xFF48 => self.obp0,
            0xFF49 => self.obp1,
            0xFF4A => self.wy,
            0xFF4B => self.wx,
            0xFF46 => self.dma,
            // Cart RAM
            0xA000..=0xBFFF => {
                match self.mbc {
                    MbcType::Mbc3 => {
                        // If RTC selected, read RTC reg; else read RAM bank 0..3
                        if let Some(sel) = self.mbc3_rtc_sel {
                            let idx = (sel - 0x08) as usize; // 0..=4
                            self.mbc3_rtc_regs.get(idx).copied().unwrap_or(0)
                        } else if self.ext_ram.is_empty() || !self.ram_enable {
                            0xFF
                        } else {
                            let bank = (self.ram_bank & 0x03) as usize;
                            let base = bank * 0x2000;
                            let off = (addr as usize - 0xA000) & 0x1FFF;
                            self.ext_ram.get(base + off).copied().unwrap_or(0xFF)
                        }
                    }
                    _ => {
                        if self.ext_ram.is_empty() || !self.ram_enable {
                            0xFF
                        } else {
                            let bank = match self.mbc {
                                MbcType::Mbc1 => {
                                    if self.mbc1_mode & 1 == 0 {
                                        0
                                    } else {
                                        (self.ram_bank & 0x03) as usize
                                    }
                                }
                                MbcType::Mbc5 => (self.ram_bank & 0x0F) as usize,
                                _ => 0,
                            };
                            let base = bank * 0x2000;
                            let off = (addr as usize - 0xA000) & 0x1FFF;
                            self.ext_ram.get(base + off).copied().unwrap_or(0xFF)
                        }
                    }
                }
            }
            // VRAM read restriction during mode 3 when LCD is on
            0x8000..=0x9FFF => {
                if (self.lcdc & 0x80) != 0 && self.ppu_mode == 3 {
                    0xFF
                } else {
                    self.ram.read(addr)
                }
            }
            // OAM read restriction during mode 2/3 when LCD is on
            0xFE00..=0xFE9F => {
                if (self.lcdc & 0x80) != 0 && (self.ppu_mode == 2 || self.ppu_mode == 3) {
                    0xFF
                } else {
                    self.ram.read(addr)
                }
            }
            _ => self.ram.read(addr),
        }
    }

    #[inline]
    pub fn write(&mut self, addr: u16, val: u8) {
        match addr {
            0x0000..=0x7FFF => {
                // If a ROM is loaded, these are MBC control writes; else write to RAM (for tests)
                if self.rom_banks > 0 {
                    self.write_mbc(addr, val);
                } else {
                    self.ram.write(addr, val);
                }
            }
            0xFF40 => {
                if !self.dbg_lcdc_first_write_done {
                    println!(
                        "[PPU] LCDC first write: {:02X} (LCD {} | BG={} WIN={} OBJ={})",
                        val,
                        if (val & 0x80) != 0 { "ON" } else { "OFF" },
                        (val & 0x01) != 0,
                        (val & 0x20) != 0,
                        (val & 0x02) != 0
                    );
                    self.dbg_lcdc_first_write_done = true;
                }
                let prev = self.lcdc;
                self.lcdc = val;
                if (prev & 0x80) == 0 && (val & 0x80) != 0 {
                    println!("[PPU] LCDC turned ON (bit7 rising edge)");
                }
            }
            0xFF41 => self.stat_w = val & 0x78, // only bits 3..6 writable
            0xFF42 => self.scy = val,
            0xFF43 => self.scx = val,
            0xFF44 => {
                // writing any value resets LY to 0 and line cycle to start (mode 2)
                self.ly = 0;
                self.ppu_line_cycle = 0;
                self.ppu_mode = 2;
            }
            0xFF45 => self.lyc = val,
            0xFFFF => self.ie = val,
            0xFF0F => self.ifl = val & 0x1F,
            0xFF00 => self.p1 = val,
            0xFF04 => {
                // writing any value resets DIV
                self.div = 0x00;
                self.div_counter = 0;
            }
            0xFF05 => self.tima = val,
            0xFF06 => self.tma = val,
            0xFF07 => self.tac = val & 0x07,
            0xFF46 => {
                // Start OAM DMA: copy 160 bytes from (val << 8) .. (val << 8) + 0x9F to 0xFE00..0xFE9F
                self.dma = val;
                let src_base = (val as u16) << 8;
                for i in 0..160u16 {
                    let b = self.read(src_base.wrapping_add(i));
                    // honor OAM write restriction is ignored during DMA (hardware writes still succeed)
                    self.ram.write(0xFE00u16.wrapping_add(i), b);
                }
            }
            0xFF47 => self.bgp = val,
            0xFF48 => self.obp0 = val,
            0xFF49 => self.obp1 = val,
            0xFF4A => self.wy = val,
            0xFF4B => self.wx = val,
            // Cart RAM / RTC
            0xA000..=0xBFFF => {
                match self.mbc {
                    MbcType::Mbc3 => {
                        if let Some(sel) = self.mbc3_rtc_sel {
                            let idx = (sel - 0x08) as usize;
                            if idx < 5 {
                                self.mbc3_rtc_regs[idx] = val;
                            }
                        } else if !(self.ext_ram.is_empty() || !self.ram_enable) {
                            let bank = (self.ram_bank & 0x03) as usize;
                            let base = bank * 0x2000;
                            let off = (addr as usize - 0xA000) & 0x1FFF;
                            if base + off < self.ext_ram.len() {
                                self.ext_ram[base + off] = val;
                            }
                        }
                    }
                    _ => {
                        if self.ext_ram.is_empty() || !self.ram_enable {
                            // ignore
                        } else {
                            let bank = match self.mbc {
                                MbcType::Mbc1 => {
                                    if self.mbc1_mode & 1 == 0 {
                                        0
                                    } else {
                                        (self.ram_bank & 0x03) as usize
                                    }
                                }
                                MbcType::Mbc5 => (self.ram_bank & 0x0F) as usize,
                                _ => 0,
                            };
                            let base = bank * 0x2000;
                            let off = (addr as usize - 0xA000) & 0x1FFF;
                            if base + off < self.ext_ram.len() {
                                self.ext_ram[base + off] = val;
                            }
                        }
                    }
                }
            }
            // VRAM write restriction during mode 3 when LCD is on
            0x8000..=0x9FFF => {
                if (self.lcdc & 0x80) != 0 && self.ppu_mode == 3 { /* ignore */
                } else {
                    if !self.dbg_vram_first_write_done {
                        println!(
                            "[PPU] First VRAM write @{:04X} = {:02X} | LCDC={:02X} LY={} STAT={:02X}",
                            addr,
                            val,
                            self.lcdc,
                            self.ly,
                            0x80 | (self.stat_w & 0x78)
                                | (if self.ly == self.lyc { 0x04 } else { 0 })
                                | (self.ppu_mode & 0x03)
                        );
                        self.dbg_vram_first_write_done = true;
                    }
                    self.ram.write(addr, val)
                }
            }
            // OAM write restriction during mode 2/3 when LCD is on
            0xFE00..=0xFE9F => {
                if (self.lcdc & 0x80) != 0 && (self.ppu_mode == 2 || self.ppu_mode == 3) { /* ignore */
                } else {
                    self.ram.write(addr, val)
                }
            }
            _ => self.ram.write(addr, val),
        }
    }

    // Optional helpers
    pub fn set_joypad_state(&mut self, p1: u8) {
        self.p1 = p1;
    }
    #[allow(dead_code)]
    pub fn request_interrupt(&mut self, mask: u8) {
        self.ifl |= mask & 0x1F;
    }

    #[inline]
    fn tima_period_cycles(&self) -> u32 {
        match self.tac & 0x03 {
            0x00 => 1024, // 4096 Hz
            0x01 => 16,   // 262144 Hz
            0x02 => 64,   // 65536 Hz
            0x03 => 256,  // 16384 Hz
            _ => 1024,
        }
    }

    pub fn step(&mut self, cycles: u64) {
        let c = cycles as u32;
        // DIV at 16384 Hz -> every 256 CPU cycles
        self.div_counter = self.div_counter.wrapping_add(c);
        if self.div_counter >= 256 {
            let inc = self.div_counter / 256;
            self.div = self.div.wrapping_add((inc & 0xFF) as u8);
            self.div_counter %= 256;
        }

        // TIMA if enabled
        if (self.tac & 0x04) != 0 {
            let period = self.tima_period_cycles();
            self.tima_counter = self.tima_counter.wrapping_add(c);
            while self.tima_counter >= period {
                self.tima_counter -= period;
                if self.tima == 0xFF {
                    self.tima = self.tma; // reload
                    self.ifl |= 0x04; // request timer interrupt
                } else {
                    self.tima = self.tima.wrapping_add(1);
                }
            }
        }

        // PPU timing: only when LCD is on (LCDC bit 7)
        if (self.lcdc & 0x80) != 0 {
            let mut remain = c;
            while remain > 0 {
                // Determine current mode boundaries for this line
                let (mode, boundary) = if self.ly >= 144 {
                    (1u8, 456u32)
                } else if self.ppu_line_cycle < 80 {
                    (2u8, 80u32)
                } else if self.ppu_line_cycle < 252 {
                    (3u8, 252u32)
                } else {
                    (0u8, 456u32)
                };
                // Handle mode transition
                if mode != self.ppu_mode {
                    // Mode change: fire STAT interrupts as needed
                    match mode {
                        2 => {
                            if (self.stat_w & 0x20) != 0 {
                                self.ifl |= 0x02;
                            }
                        } // OAM
                        1 => {
                            if (self.stat_w & 0x10) != 0 {
                                self.ifl |= 0x02;
                            }
                        } // VBlank
                        0 => {
                            // Entering HBlank; if we just finished mode 3 on a visible line, render it
                            if self.ppu_mode == 3 && self.ly < 144 {
                                self.render_scanline();
                            }
                            if (self.stat_w & 0x08) != 0 {
                                self.ifl |= 0x02;
                            }
                        } // HBlank
                        3 => {
                            if (self.stat_w & 0x10) != 0 {
                                // Mode 3 doesn't have its own STAT bit; keep as is
                            }
                        }
                        _ => {}
                    }
                    self.ppu_mode = mode;
                }
                // Advance by a small step to reach the boundary
                let step = (boundary - self.ppu_line_cycle).min(remain);
                self.ppu_line_cycle += step;
                remain -= step;
                if self.ppu_line_cycle >= 456 {
                    // End of line
                    self.ppu_line_cycle = 0;
                    self.ly = self.ly.wrapping_add(1);
                    if self.ly == 144 {
                        // Entering VBlank
                        self.ifl |= 0x01; // VBlank IF
                        if (self.stat_w & 0x10) != 0 {
                            self.ifl |= 0x02;
                        } // STAT VBlank
                    }
                    if self.ly > 153 {
                        self.ly = 0;
                    }
                    // STAT coincidence
                    if (self.stat_w & 0x40) != 0 && self.ly == self.lyc {
                        self.ifl |= 0x02;
                    }
                    // Entering VBlank is handled when mode becomes 1 at ly>=144 in next loop iteration
                    // Keep mode for next line start (will be recalculated at top)
                }
            }
            // Ensure coincidence interrupt is raised if LY==LYC after processing this batch
            if (self.stat_w & 0x40) != 0 && self.ly == self.lyc {
                self.ifl |= 0x02;
            }
        } else {
            // LCD off: reset PPU timing state
            self.ppu_mode = 0;
            self.ppu_line_cycle = 0;
        }
    }
}
