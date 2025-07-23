use crate::core::mmu::mmu::MMU;

/// PPU (像素處理單元) 負責 Game Boy 的圖形渲染
pub struct PPU {
    pub mmu: *mut MMU, // 直接持有裸指標，僅供內部存取，不負責釋放
    pub lcd_enabled: bool,
    pub framebuffer: Vec<(u8, u8, u8)>,
    // [LOG] 每次 frame buffer 更新時插入：
    // log_to_file!("[PPU] framebuffer updated, size={}", self.framebuffer.len());
    pub bgp: u8,
    pub obp0: u8,
    pub obp1: u8,
    pub scx: u8,
    pub scy: u8,
    pub wy: u8,
    pub wx: u8,
    pub lcdc: u8,
    pub last_frame_time: std::time::Instant,
    pub fps_counter: u32,
    pub mode: u8,
    pub ly: u8,
    pub lyc: u8,
    pub stat: u8,
    pub dots: u32,
    pub oam: [u8; 0xA0],
    pub vram: *mut crate::core::ppu::vram::VRAM, // 直接持有裸指標
}
impl PPU {
    /// 取得 VRAM 內部像素資料（unsafe，僅供 dump/debug 用）
    pub fn get_vram_data(&self) -> &[u8] {
        unsafe { &(*self.vram).data }
    }
    /// 取得目前畫面像素索引（供 SDL display 使用）
    pub fn get_framebuffer_indices(&self) -> Vec<u8> {
        // 若需單色索引，回傳 R 分量
        self.framebuffer.iter().map(|rgb| rgb.0).collect()
    }
    /// 執行一個 PPU step（等同 tick）
    pub fn step(&mut self) {
        self.tick();
    }

    /// 連續執行 n 次 PPU tick
    pub fn run(&mut self, cycles: usize) {
        for _ in 0..cycles {
            self.tick();
        }
    }
    pub fn new(mmu: &mut MMU, vram: &mut crate::core::ppu::vram::VRAM) -> Self {
        Self {
            mmu: mmu as *mut MMU,
            lcd_enabled: false,
            framebuffer: vec![(224, 248, 208); 160 * 144],
            bgp: 0xE4,
            obp0: 0xFF,
            obp1: 0xFF,
            scx: 0,
            scy: 0,
            wy: 0,
            wx: 0,
            lcdc: 0x91,
            last_frame_time: std::time::Instant::now(),
            fps_counter: 0,
            mode: 0,
            ly: 0,
            lyc: 0,
            stat: 0,
            dots: 0,
            oam: [0; 0xA0],
            vram: vram as *mut crate::core::ppu::vram::VRAM,
        }
    }

    pub fn set_lcd_enabled(&mut self, enabled: bool) {
        self.lcd_enabled = enabled;
    }

    pub fn is_lcd_enabled(&self) -> bool {
        self.lcd_enabled
    }

    /// 執行一個 PPU 週期 (tick)
    pub fn tick(&mut self) {
        // === Sprite (OBJ) 渲染 ===
        // 只處理 8x8 sprite，未處理優先權/翻轉/遮蔽
        let w = 160;
        let h = 144;
        let vram_ref = unsafe { &*self.vram };
        // --- 初始化 framebuffer ---
        let w = 160;
        let h = 144;
        self.framebuffer.clear();
        self.framebuffer.resize(w * h, (224, 248, 208)); // 白底
        let vram_ref = unsafe { &*self.vram };
        // --- LCDC 判斷 ---
        if self.lcdc & 0x80 == 0 {
            // LCDC bit 7: display disable
            return;
        }
        let bg_enable = self.lcdc & 0x01 != 0;
        let sprite_enable = self.lcdc & 0x02 != 0;
        let window_enable = self.lcdc & 0x20 != 0;
        // --- Palette設置：Game Boy 綠色系（彩色化）---
        let color_map: [(u8, u8, u8); 4] = [
            (224, 248, 208), // 白
            (136, 192, 112), // 淺綠
            (52, 104, 86),   // 深綠
            (8, 24, 32),     // 黑
        ];
        let bgp = self.bgp;
        let palette = [
            (bgp >> 0) & 0x03,
            (bgp >> 2) & 0x03,
            (bgp >> 4) & 0x03,
            (bgp >> 6) & 0x03,
        ];
        // --- BG/Window tile map/data base addr ---
        let bg_tile_map_addr = if self.lcdc & 0x08 != 0 {
            0x1C00
        } else {
            0x1800
        };
        let bg_tile_data_addr = if self.lcdc & 0x10 != 0 {
            0x0000
        } else {
            0x0800
        };
        // --- 主迴圈 ---
        for y in 0..h {
            let scy = self.scy as usize;
            let scx = self.scx as usize;
            let ly = (y + scy) & 0xFF;
            for x in 0..w {
                let mut rgb = color_map[0];
                // --- Window decode（優先）---
                if window_enable
                    && y >= self.wy as usize
                    && x >= (self.wx as usize).saturating_sub(7)
                {
                    let win_tile_map_addr = if self.lcdc & 0x40 != 0 {
                        0x1C00
                    } else {
                        0x1800
                    };
                    let win_y = y - self.wy as usize;
                    let win_x = x - (self.wx as usize).saturating_sub(7);
                    let tile_map_x = win_x / 8;
                    let tile_map_y = win_y / 8;
                    let tile_map_index = tile_map_y * 32 + tile_map_x;
                    let tile_map_addr = win_tile_map_addr + tile_map_index;
                    let tile_num = vram_ref.data[tile_map_addr];
                    let tile_addr = if self.lcdc & 0x10 != 0 {
                        0x0000 + (tile_num as usize) * 16
                    } else {
                        let idx = tile_num as i8 as i16;
                        (0x0800 as isize + (idx * 16) as isize) as usize
                    };
                    let tile_y = win_y % 8;
                    let byte1 = vram_ref
                        .data
                        .get(tile_addr + tile_y * 2)
                        .copied()
                        .unwrap_or(0);
                    let byte2 = vram_ref
                        .data
                        .get(tile_addr + tile_y * 2 + 1)
                        .copied()
                        .unwrap_or(0);
                    let bit = 7 - (win_x % 8);
                    let color_num = ((byte2 >> bit) & 1) << 1 | ((byte1 >> bit) & 1);
                    let pal_idx = palette[color_num as usize];
                    rgb = color_map[pal_idx as usize];
                } else if bg_enable {
                    // --- BG decode ---
                    let bg_tile_map_addr = if self.lcdc & 0x08 != 0 {
                        0x1C00
                    } else {
                        0x1800
                    };
                    let bg_tile_data_addr = if self.lcdc & 0x10 != 0 {
                        0x0000
                    } else {
                        0x0800
                    };
                    let lx = (x + scx) & 0xFF;
                    let tile_map_x = (lx / 8) % 32; // tile map 寬度 32
                    let tile_map_y = (ly / 8) % 32; // tile map 高度 32
                    let tile_map_index = tile_map_y * 32 + tile_map_x;
                    let tile_map_addr = bg_tile_map_addr + tile_map_index;
                    let tile_num = vram_ref.data[tile_map_addr];
                    let tile_addr = if self.lcdc & 0x10 != 0 {
                        bg_tile_data_addr + (tile_num as usize) * 16
                    } else {
                        let idx = tile_num as i8 as i16;
                        (bg_tile_data_addr as isize + (idx * 16) as isize) as usize
                    };
                    let tile_y = ly % 8;
                    let mut rgb = color_map[0];
                    let mut error = false;
                    if tile_addr + tile_y * 2 + 1 < vram_ref.data.len() {
                        let byte1 = vram_ref.data[tile_addr + tile_y * 2];
                        let byte2 = vram_ref.data[tile_addr + tile_y * 2 + 1];
                        let bit = 7 - (lx % 8);
                        let color_num = (((byte2 >> bit) & 1) << 1) | ((byte1 >> bit) & 1);
                        let pal_idx = if (color_num as usize) < palette.len() {
                            palette[color_num as usize]
                        } else {
                            error = true;
                            0
                        };
                        let pal_idx = if (pal_idx as usize) < color_map.len() {
                            pal_idx
                        } else {
                            error = true;
                            0
                        };
                        rgb = color_map[pal_idx as usize];
                        // Debug: 前 32 像素輸出 tile_map_addr, tile_num, tile_addr, byte1, byte2, color_num, pal_idx, rgb
                        if y == 0 && x < 32 {
                            use std::fs::OpenOptions;
                            use std::io::Write;
                            let mut log = OpenOptions::new()
                                .create(true)
                                .append(true)
                                .open("logs/emulator.log")
                                .unwrap();
                            writeln!(log, "BG[{}] tile_map_addr={:04X} tile_num={:02X} tile_addr={:04X} byte1={:02X} byte2={:02X} color_num={} pal_idx={} rgb={:?}{}",
                                x,
                                tile_map_addr,
                                tile_num,
                                tile_addr,
                                byte1,
                                byte2,
                                color_num,
                                pal_idx,
                                rgb,
                                if error {" [ERR]"} else {""}
                            ).ok();
                        }
                    } else {
                        // tile_addr 超出範圍，填白色
                        rgb = color_map[0];
                        if y == 0 && x < 32 {
                            use std::fs::OpenOptions;
                            use std::io::Write;
                            let mut log = OpenOptions::new()
                                .create(true)
                                .append(true)
                                .open("logs/emulator.log")
                                .unwrap();
                            writeln!(log, "BG[{}] tile_map_addr={:04X} tile_num={:02X} tile_addr={:04X} [OUT OF VRAM]", x, tile_map_addr, tile_num, tile_addr).ok();
                        }
                    }
                }
                // --- Sprite decode（疊加）---
                if sprite_enable {
                    for i in 0..40 {
                        let oam_idx = i * 4;
                        if oam_idx + 3 >= self.oam.len() {
                            break;
                        }
                        let sprite_y = self.oam[oam_idx] as isize - 16;
                        let sprite_x = self.oam[oam_idx + 1] as isize - 8;
                        let tile_idx = self.oam[oam_idx + 2] as usize;
                        let attr = self.oam[oam_idx + 3];
                        let obj_palette = if (attr & 0x10) == 0 {
                            self.obp0
                        } else {
                            self.obp1
                        };
                        let pal_idx = |color_num: u8| (obj_palette >> (color_num * 2)) & 0x03;
                        let sprite_color_map: [(u8, u8, u8); 4] = [
                            (224, 248, 208), // 白
                            (136, 192, 112), // 淺綠
                            (52, 104, 86),   // 深綠
                            (8, 24, 32),     // 黑
                        ];
                        // 判斷是否在此(y,x)座標
                        if (y as isize) >= sprite_y && (y as isize) < (sprite_y + 8) {
                            let sprite_line = (y as isize - sprite_y) as usize;
                            let tile_addr = 0x0000 + tile_idx * 16 + sprite_line * 2;
                            let byte1 = vram_ref.data.get(tile_addr).copied().unwrap_or(0);
                            let byte2 = vram_ref.data.get(tile_addr + 1).copied().unwrap_or(0);
                            for sx in 0..8 {
                                let bit = 7 - sx;
                                let color_num = ((byte2 >> bit) & 1) << 1 | ((byte1 >> bit) & 1);
                                if color_num == 0 {
                                    continue;
                                }
                                let px = sprite_x + sx;
                                if px < 0 || px >= w as isize {
                                    continue;
                                }
                                if px as usize == x {
                                    let rgb_sprite = sprite_color_map[pal_idx(color_num) as usize];
                                    rgb = rgb_sprite;
                                }
                            }
                        }
                    }
                }
                self.framebuffer[y * w + x] = rgb;
            }
        }
        let bgp = if self.bgp == 0 { 0xE4 } else { self.bgp };
        let palette = [
            (bgp >> 0) & 0x03,
            (bgp >> 2) & 0x03,
            (bgp >> 4) & 0x03,
            (bgp >> 6) & 0x03,
        ];
        for y in 0..h {
            let scy = self.scy as usize;
            let scx = self.scx as usize;
            let ly = (y + scy) & 0xFF;
            for x in 0..w {
                let rgb: (u8, u8, u8);
                // --- Window enable ---
                if (self.lcdc & 0x20) != 0
                    && y >= self.wy as usize
                    && x >= (self.wx as usize).saturating_sub(7)
                {
                    // Window tile map: LCDC bit 6 (0=0x9800, 1=0x9C00)
                    let win_tile_map_addr = if self.lcdc & 0x40 != 0 {
                        0x1C00
                    } else {
                        0x1800
                    };
                    let win_y = y - self.wy as usize;
                    let win_x = x - (self.wx as usize).saturating_sub(7);
                    let tile_map_x = win_x / 8;
                    let tile_map_y = win_y / 8;
                    let tile_map_index = tile_map_y * 32 + tile_map_x;
                    let tile_map_addr = win_tile_map_addr + tile_map_index;
                    let tile_num = vram_ref.data[tile_map_addr];
                    let tile_addr = if self.lcdc & 0x10 != 0 {
                        bg_tile_data_addr + (tile_num as usize) * 16
                    } else {
                        let idx = (tile_num as i8 as i16) as isize;
                        (bg_tile_data_addr as isize + (idx * 16)) as usize
                    };
                    let tile_y = win_y % 8;
                    let byte1 = vram_ref
                        .data
                        .get(tile_addr + tile_y * 2)
                        .copied()
                        .unwrap_or(0);
                    let byte2 = vram_ref
                        .data
                        .get(tile_addr + tile_y * 2 + 1)
                        .copied()
                        .unwrap_or(0);
                    let bit = 7 - (win_x % 8);
                    let color_num = ((byte2 >> bit) & 1) << 1 | ((byte1 >> bit) & 1);
                    let pal_idx = palette[color_num as usize];
                    rgb = color_map[pal_idx as usize];
                } else {
                    // BG tile decode
                    let lx = (x + scx) & 0xFF;
                    let tile_map_x = lx / 8;
                    let tile_map_y = ly / 8;
                    let tile_map_index = tile_map_y * 32 + tile_map_x;
                    let tile_map_addr = bg_tile_map_addr + tile_map_index;
                    let tile_num = vram_ref.data[tile_map_addr];
                    let tile_addr = if self.lcdc & 0x10 != 0 {
                        bg_tile_data_addr + (tile_num as usize) * 16
                    } else {
                        let idx = (tile_num as i8 as i16) as isize;
                        (bg_tile_data_addr as isize + (idx * 16)) as usize
                    };
                    let tile_y = ly % 8;
                    let byte1 = vram_ref
                        .data
                        .get(tile_addr + tile_y * 2)
                        .copied()
                        .unwrap_or(0);
                    let byte2 = vram_ref
                        .data
                        .get(tile_addr + tile_y * 2 + 1)
                        .copied()
                        .unwrap_or(0);
                    let bit = 7 - (lx % 8);
                    let color_num = ((byte2 >> bit) & 1) << 1 | ((byte1 >> bit) & 1);
                    let pal_idx = palette[color_num as usize];
                    rgb = color_map[pal_idx as usize];
                }
                // 強化 debug log：前 32 像素 rgb
                if y == 0 && x < 32 {
                    use std::fs::OpenOptions;
                    use std::io::Write;
                    let mut log = OpenOptions::new()
                        .create(true)
                        .append(true)
                        .open("logs/emulator.log")
                        .unwrap();
                    writeln!(
                        log,
                        "[PPU] y={} x={} rgb=({},{},{})",
                        y, x, rgb.0, rgb.1, rgb.2
                    )
                    .ok();
                }
                self.framebuffer.push(rgb);
            }
        }
    }
    /// 取得目前畫面緩衝區
    pub fn get_framebuffer(&self) -> &[(u8, u8, u8)] {
        // 若不足則回傳全白畫面
        if self.framebuffer.len() < 160 * 144 {
            static WHITE_FB: [(u8, u8, u8); 160 * 144] = [(224, 248, 208); 160 * 144];
            &WHITE_FB
        } else {
            &self.framebuffer
        }
    }
}
