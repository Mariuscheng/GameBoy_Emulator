// Bus IO (Joypad/Timer/PPU) 相關欄位與邏輯

use crate::GB::types::*;
pub struct BusIO {
    pub ie: u8,
    pub ifl: u8,
    pub p1: u8,
    pub p1_sel: u8,
    pub joyp_dpad: u8,
    pub joyp_btns: u8,
    pub div: u8,
    pub tima: u8,
    pub tma: u8,
    pub tac: u8,
    pub lcdc: u8,
    pub stat_w: u8,
    pub scy: u8,
    pub scx: u8,
    pub ly: u8,
    pub lyc: u8,
    pub bgp: u8,
    pub obp0: u8,
    pub obp1: u8,
    pub wy: u8,
    pub wx: u8,
    pub ppu_line_cycle: u32,
    pub ppu_mode: u8,
    pub lcd_on_delay: u32,
    pub framebuffer: [u8; 160 * 144],
    // Serial
    pub sb: u8,
    pub sc: u8,
    pub div_counter: u16,
    pub div_sub: u8,
    pub tima_reload_delay: u32,
    pub win_line: u8,
    pub line_base_scx: u8,
    pub line_base_scy: u8,
    pub line_base_wx: u8,
    pub line_base_bgp: u8,
    pub scan_events: Vec<RegEvent>,
    pub dbg_lcdc_first_write_done: bool,
    pub dbg_vram_first_write_done: bool,
    pub dma_active: bool,
    pub dma_pos: u16,
    pub dma_start_delay: u32,
    pub dma_cycle_accum: u32,
}

impl BusIO {
    pub fn new() -> Self {
        Self {
            ie: 0x00,
            ifl: 0x00,
            p1: 0xFF,
            p1_sel: 0x30,
            joyp_dpad: 0x0F,
            joyp_btns: 0x0F,
            div: 0x00,
            tima: 0x00,
            tma: 0x00,
            tac: 0x00,
            lcdc: 0x00,
            stat_w: 0x00,
            scy: 0x00,
            scx: 0x00,
            ly: 0x00,
            lyc: 0x00,
            bgp: 0xFC,
            obp0: 0xFF,
            obp1: 0xFF,
            wy: 0x00,
            wx: 0x00,
            ppu_line_cycle: 0,
            ppu_mode: 2,
            lcd_on_delay: 0,
            framebuffer: [0; 160 * 144],
            sb: 0,
            sc: 0,
            div_counter: 0,
            div_sub: 0,
            tima_reload_delay: 0,
            win_line: 0,
            line_base_scx: 0,
            line_base_scy: 0,
            line_base_wx: 0,
            line_base_bgp: 0xFC,
            scan_events: Vec::with_capacity(64),
            dbg_lcdc_first_write_done: false,
            dbg_vram_first_write_done: false,
            dma_active: false,
            dma_pos: 0,
            dma_start_delay: 0,
            dma_cycle_accum: 0,
        }
    }
    // TODO: 移植 PPU、Timer、Joypad、DMA、framebuffer 等方法
}

impl BusIO {
    // 這裡可放原本 BUS.rs 內所有 IO 相關方法
    pub fn step(
        &mut self,
        cycles: u64,
        mem: &mut crate::GB::bus_mem::BusMem,
        _apu: &mut crate::GB::bus_apu::BusAPU,
    ) {
        let c = cycles as u32;

        // 更新 DIV (每 256 CPU cycles 會加 1) 與 TIMA (簡化：若啟用則每 1024 cycles 加 1)
        self.div_counter = self.div_counter.wrapping_add(c as u16);
        while self.div_counter >= 256 {
            self.div_counter -= 256;
            self.div = self.div.wrapping_add(1);
        }
        if (self.tac & 0x04) != 0 {
            // 極簡：忽略真實頻率選擇，只做固定遞增
            self.dma_cycle_accum = self.dma_cycle_accum.wrapping_add(c);
            while self.dma_cycle_accum >= 1024 {
                self.dma_cycle_accum -= 1024;
                let (new, overflow) = self.tima.overflowing_add(1);
                if overflow {
                    self.tima = self.tma;
                    // 設定定時中斷 (IF bit2)
                    self.ifl |= 0x04;
                } else {
                    self.tima = new;
                }
            }
        }

        // DMA 啟動延遲處理 & 簡化 OAM 複製 (不做來源 gating)
        if self.dma_active {
            if self.dma_start_delay > 0 {
                if c >= self.dma_start_delay {
                    self.dma_start_delay = 0;
                } else {
                    self.dma_start_delay -= c;
                }
            } else {
                // 每 4 cycles 複製一筆 (簡化)
                self.dma_cycle_accum += c;
                while self.dma_cycle_accum >= 4 && self.dma_pos < 160 {
                    self.dma_cycle_accum -= 4;
                    // 實際應從來源高位 FF46<<8 + dma_pos 讀取；此處用 0x0000 當 dummy
                    let src = (0x0000u16).wrapping_add(self.dma_pos);
                    let val = mem.ram.read(src);
                    // OAM 範圍：FE00 + pos
                    mem.ram.write(0xFE00u16 + self.dma_pos, val);
                    self.dma_pos += 1;
                }
                if self.dma_pos >= 160 {
                    self.dma_active = false;
                }
            }
        }

        // PPU 掃描線推進與渲染
        if (self.lcdc & 0x80) != 0 {
            let mut remain = c;
            while remain > 0 {
                let (mode, boundary) = if self.ly >= 144 {
                    (1u8, 456u32)
                } else if self.ppu_line_cycle < 80 {
                    (2u8, 80u32)
                } else {
                    let mode3_len = 172u32 + ((self.scx & 0x07) as u32);
                    let mode3_end = 80u32 + mode3_len;
                    if self.ppu_line_cycle < mode3_end {
                        (3u8, mode3_end)
                    } else {
                        (0u8, 456u32)
                    }
                };
                // Mode 切換時
                if mode != self.ppu_mode {
                    self.ppu_mode = mode;
                }
                let step = (boundary - self.ppu_line_cycle).min(remain);
                self.ppu_line_cycle += step;
                remain -= step;
                // 每條可見掃描線都強制呼叫 render_scanline
                if self.ppu_line_cycle >= 456 {
                    if self.ly < 144 {
                        Self::render_scanline(self, mem);
                    }
                    if self.ly == 143 {
                        // 進入 VBlank 的第一條，觸發 VBlank IF bit0
                        self.ifl |= 0x01;
                    }
                    self.ppu_line_cycle = 0;
                    self.ly = self.ly.wrapping_add(1);
                    if self.ly > 153 {
                        self.ly = 0;
                    }
                }
            }
        } else {
            self.ppu_mode = 0;
            self.ppu_line_cycle = 0;
            self.win_line = 0;
            self.ly = 0;
        }
    }

    /// 最簡單的背景掃描線渲染（僅灰階）
    fn render_scanline(&mut self, mem: &crate::GB::bus_mem::BusMem) {
        let y = self.ly as usize;
        if y >= 144 {
            return;
        }
        let mut shades = [0u8; 160];
        let mut any_nonzero = false;
        // BG enable
        if (self.lcdc & 0x01) != 0 {
            let scy = self.scy as u16;
            let scx = self.scx as u16;
            let v = ((self.ly as u16).wrapping_add(scy)) & 0xFF;
            let tilemap = if (self.lcdc & 0x08) != 0 {
                0x9C00
            } else {
                0x9800
            };
            let signed = (self.lcdc & 0x10) == 0;
            let row_in_tile = (v & 7) as u16;
            let tile_row = ((v >> 3) & 31) as u16;
            for x in 0..160u16 {
                let h = (x.wrapping_add(scx)) & 0xFF;
                let tile_col = ((h >> 3) & 31) as u16;
                let map_index = tile_row * 32 + tile_col;
                let tile_id = mem.ram.read(tilemap + map_index);
                let tile_addr = if signed {
                    let idx = tile_id as i8 as i16;
                    let base = 0x9000i32 + (idx as i32) * 16;
                    base as u16
                } else {
                    0x8000u16 + (tile_id as u16) * 16
                };
                let lo = mem.ram.read(tile_addr + row_in_tile * 2);
                let hi = mem.ram.read(tile_addr + row_in_tile * 2 + 1);
                let bit = 7 - ((h & 7) as u8);
                let lo_b = (lo >> bit) & 1;
                let hi_b = (hi >> bit) & 1;
                let color = (hi_b << 1) | lo_b;
                let shade = (self.bgp >> (color * 2)) & 0x03;
                shades[x as usize] = shade;
                if shade != 0 {
                    any_nonzero = true;
                }
            }
        } else {
            for x in 0..160usize {
                shades[x] = 0;
            }
        }
        let base = y * 160;
        for x in 0..160usize {
            self.framebuffer[base + x] = shades[x];
        }
        if y == 0 || y == 72 || y == 143 {
            println!(
                "[PPU] render_scanline y={} any_nonzero={} LCDC={:02X} BGP={:02X}",
                y, any_nonzero, self.lcdc, self.bgp
            );
        }
    }
    pub fn is_dma_active(&self) -> bool {
        self.dma_active && self.dma_pos < 160 && self.dma_start_delay == 0
    }
    pub fn get_ie_raw(&self) -> u8 {
        self.ie
    }
    pub fn get_if_raw(&self) -> u8 {
        self.ifl | 0xE0
    }
    pub fn set_if_raw(&mut self, v: u8) {
        self.ifl = v & 0x1F;
    }
    pub fn set_joypad_rows(&mut self, dpad: u8, btns: u8) {
        self.joyp_dpad = dpad & 0x0F;
        self.joyp_btns = btns & 0x0F;
    }
    pub fn framebuffer(&self) -> &[u8] {
        &self.framebuffer
    }
}
