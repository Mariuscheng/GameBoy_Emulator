// Bus 記憶體相關欄位與邏輯
use crate::GB::types::*;
use crate::GB::RAM::RAM;

pub struct BusMem {
    pub ram: RAM,
    pub rom: Vec<u8>,
    pub rom_banks: usize,
    pub mbc: MbcType,
    pub ram_enable: bool,
    pub rom_bank: u16,
    pub ram_bank: u8,
    pub ext_ram: Vec<u8>,
    pub mbc1_bank_low5: u8,
    pub mbc1_bank_high2: u8,
    pub mbc1_mode: u8,
    pub mbc3_rtc_sel: Option<u8>,
    pub mbc3_rtc_regs: [u8; 5],
}

impl BusMem {
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
        }
    }
    // TODO: 移植 load_rom, read_rom, write_mbc, mbc1_calc_bank0, mbc1_calc_bankX 等方法
}
impl BusMem {
    // 這裡可放原本 BUS.rs 內所有 RAM/ROM/MBC 相關方法
    pub fn load_rom(&mut self, data: Vec<u8>) {
        self.rom = data;
        self.rom_banks = (self.rom.len() + 0x3FFF) / 0x4000;
        let cart_type = if self.rom.len() > 0x0147 {
            self.rom[0x0147]
        } else {
            0x00
        };
        self.mbc = match cart_type {
            0x01 | 0x02 | 0x03 => MbcType::Mbc1,
            0x0F | 0x10 | 0x11 | 0x12 | 0x13 => MbcType::Mbc3,
            0x19 | 0x1A | 0x1B | 0x1C | 0x1D | 0x1E => MbcType::Mbc5,
            _ => MbcType::None,
        };
        let ram_size_code = if self.rom.len() > 0x0149 {
            self.rom[0x0149]
        } else {
            0
        };
        let ram_banks = match ram_size_code {
            0x02 => 1,
            0x03 => 4,
            0x04 => 16,
            0x05 => 8,
            _ => 0,
        };
        self.ext_ram = vec![0u8; ram_banks * 0x2000];
        self.ram_enable = false;
        self.rom_bank = 1;
        self.ram_bank = 0;
        self.mbc1_bank_low5 = 1;
        self.mbc1_bank_high2 = 0;
        self.mbc1_mode = 0;
        self.mbc3_rtc_sel = None;
        self.mbc3_rtc_regs = [0; 5];
    }

    fn mbc1_calc_bank0(&self) -> u16 {
        if self.mbc1_mode & 1 == 0 {
            0
        } else {
            ((self.mbc1_bank_high2 as u16) & 0x03) << 5
        }
    }
    fn mbc1_calc_bankX(&self) -> u16 {
        let low5 = (self.mbc1_bank_low5 as u16) & 0x1F;
        let mut bank = if self.mbc1_mode & 1 == 0 {
            (low5) | (((self.mbc1_bank_high2 as u16) & 0x03) << 5)
        } else {
            low5
        };
        if (bank & 0x1F) == 0 {
            bank |= 1;
        }
        bank
    }
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
                    let mut bank = (self.rom_bank as usize) & 0x7F;
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
                    let base = 0usize;
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
    fn write_mbc(&mut self, addr: u16, val: u8) {
        match self.mbc {
            MbcType::None => {}
            MbcType::Mbc1 => match addr {
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
                        self.ram_bank = self.mbc1_bank_high2 & 0x03;
                    }
                }
                0x6000..=0x7FFF => {
                    self.mbc1_mode = val & 0x01;
                }
                _ => {}
            },
            MbcType::Mbc3 => match addr {
                0x0000..=0x1FFF => {
                    self.ram_enable = (val & 0x0F) == 0x0A;
                }
                0x2000..=0x3FFF => {
                    let mut b = (val & 0x7F) as u16;
                    if b == 0 {
                        b = 1;
                    }
                    self.rom_bank = b;
                }
                0x4000..=0x5FFF => {
                    let v = val & 0x0F;
                    if v <= 0x03 {
                        self.ram_bank = v;
                        self.mbc3_rtc_sel = None;
                    } else if (0x08..=0x0C).contains(&v) {
                        self.mbc3_rtc_sel = Some(v);
                    }
                }
                0x6000..=0x7FFF => {}
                _ => {}
            },
            MbcType::Mbc5 => match addr {
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
                0x6000..=0x7FFF => {}
                _ => {}
            },
        }
    }

    pub fn read(
        &self,
        addr: u16,
        io: &crate::GB::bus_io::BusIO,
        _apu: &crate::GB::bus_apu::BusAPU,
    ) -> u8 {
        match addr {
            0x0000..=0x7FFF => {
                if self.rom_banks > 0 {
                    self.read_rom(addr)
                } else {
                    self.ram.read(addr)
                }
            }
            // IO mapped registers
            0xFF00 => {
                let sel = io.p1_sel & 0x30;
                let mut v = 0xC0 | sel;
                if (sel & 0x10) == 0 {
                    v = (v & 0xF0) | (io.joyp_dpad & 0x0F);
                } else if (sel & 0x20) == 0 {
                    v = (v & 0xF0) | (io.joyp_btns & 0x0F);
                } else {
                    v |= 0x0F;
                }
                v
            }
            0xFF04 => io.div,
            0xFF01 => io.sb,
            0xFF02 => io.sc | 0x7C, // 只留 bit0/bit7，其餘讀為 1
            0xFF05 => io.tima,
            0xFF06 => io.tma,
            0xFF07 => io.tac & 0x07,
            0xFF0F => io.get_if_raw(),
            0xFF40 => io.lcdc,
            0xFF41 => {
                let mut stat = io.stat_w & 0x78;
                if io.ly == io.lyc {
                    stat |= 0x04;
                }
                stat |= io.ppu_mode & 0x03;
                stat | 0x80
            }
            0xFF42 => io.scy,
            0xFF43 => io.scx,
            0xFF44 => io.ly,
            0xFF45 => io.lyc,
            0xFF47 => io.bgp,
            0xFF48 => io.obp0,
            0xFF49 => io.obp1,
            0xFF4A => io.wy,
            0xFF4B => io.wx,
            0xFFFF => io.ie,
            // APU regs mirror (minimal): 0xFF10..=0xFF3F
            0xFF10..=0xFF25 | 0xFF27..=0xFF2F | 0xFF30..=0xFF3F => {
                let idx = (addr - 0xFF10) as usize;
                // We can't read _apu here because read() has immutable &_apu. Add a harmless RAM mirror for now.
                // To keep compatibility, return RAM value; proper APU mapping is in write/read below.
                self.ram.read(addr)
            }
            0xFF26 => {
                // NR52: bit7 power, bits 0-3 channel on, bits 4-6 read 1.
                // Minimal: return 0x70 | (stored bit7)
                let stored = self.ram.read(0xFF26);
                0x70 | (stored & 0x80)
            }
            0xA000..=0xBFFF => {
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
            _ => self.ram.read(addr),
        }
    }

    pub fn write(
        &mut self,
        addr: u16,
        val: u8,
        io: &mut crate::GB::bus_io::BusIO,
        _apu: &mut crate::GB::bus_apu::BusAPU,
    ) {
        match addr {
            0x0000..=0x7FFF => {
                if self.rom_banks > 0 {
                    self.write_mbc(addr, val);
                } else {
                    self.ram.write(addr, val);
                }
            }
            0x8000..=0x9FFF => {
                self.ram.write(addr, val);
            }
            // IO mapped registers
            0xFF00 => {
                io.p1_sel = val & 0x30;
            }
            0xFF04 => {
                io.div = 0;
                io.div_counter = 0;
                io.div_sub = 0;
            }
            0xFF05 => {
                io.tima = val;
            }
            0xFF06 => {
                io.tma = val;
            }
            0xFF07 => {
                io.tac = val & 0x07;
            }
            0xFF0F => {
                io.set_if_raw(val);
            }
            0xFF40 => {
                io.lcdc = val;
            }
            0xFF41 => {
                io.stat_w = val & 0x78;
            }
            0xFF42 => {
                io.scy = val;
            }
            0xFF43 => {
                io.scx = val;
            }
            0xFF44 => {
                io.ly = 0;
                io.ppu_line_cycle = 0;
            }
            0xFF45 => {
                io.lyc = val;
            }
            0xFF46 => {
                io.dma_active = true;
                io.dma_pos = 0;
                io.dma_start_delay = 1;
                io.dma_cycle_accum = 0;
            }
            0xFF47 => {
                io.bgp = val;
            }
            0xFF48 => {
                io.obp0 = val;
            }
            0xFF49 => {
                io.obp1 = val;
            }
            0xFF4A => {
                io.wy = val;
            }
            0xFF4B => {
                io.wx = val;
            }
            // Serial
            0xFF01 => {
                io.sb = val;
            }
            0xFF02 => {
                io.sc = val & 0x81; // bit7=start, bit0=clock select
                if (io.sc & 0x80) != 0 {
                    let ch = io.sb as char;
                    print!("{}", ch);
                    let _ = std::io::Write::flush(&mut std::io::stdout());
                    io.ifl |= 0x08; // serial interrupt
                    io.sc &= !0x80; // clear start (transfer complete)
                }
            }
            // APU regs mirror (minimal): write-through to RAM for now
            0xFF10..=0xFF25 | 0xFF27..=0xFF2F | 0xFF30..=0xFF3F => {
                self.ram.write(addr, val);
            }
            0xFF26 => {
                // NR52 power control: store to RAM mirror; if power off, zero channel regs minimally
                self.ram.write(addr, val & 0x80);
                if (val & 0x80) == 0 {
                    // power off clears other APU regs (simplified)
                    for a in 0xFF10u16..=0xFF3Fu16 {
                        if a != 0xFF26 {
                            self.ram.write(a, 0);
                        }
                    }
                }
            }
            0xFFFF => {
                io.ie = val;
            }
            0xA000..=0xBFFF => {
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
            _ => self.ram.write(addr, val),
        }
    }
}
