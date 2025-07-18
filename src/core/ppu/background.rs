//! Background renderer, generates background layer pixels

use super::pixel::BgPixel;
use crate::core::error::Result;
use crate::core::mmu::MMU;

const SCREEN_WIDTH: usize = 160;
// 移除未使用的常量

#[derive(Debug)]
pub struct BackgroundRenderer {
    scroll_x: u8,
    scroll_y: u8,
}

impl BackgroundRenderer {
    pub fn new() -> Self {
        Self {
            scroll_x: 0,
            scroll_y: 0,
        }
    }

    /// Generate background pixel color IDs (0~3) for one scanline
    pub fn render_line(&mut self, line: u8, mmu: &MMU) -> Result<Vec<BgPixel>> {
        let mut line_pixels = vec![BgPixel::new(0, 0); SCREEN_WIDTH];

        // 讀取控制寄存器
        let lcdc = mmu.read_byte(0xFF40)?;
        if lcdc & 0x01 == 0 {
            // 背景被禁用時返回空白掃描線
            return Ok(line_pixels);
        }

        // 讀取捲動值
        self.scroll_x = mmu.read_byte(0xFF43)?;
        self.scroll_y = mmu.read_byte(0xFF42)?;

        // 讀取調色盤
        let bgp = mmu.read_byte(0xFF47)?;

        // 計算實際Y座標（考慮捲動）
        let real_y = (line as u16 + self.scroll_y as u16) & 0xFF;
        let tile_row = (real_y >> 3) as u16;
        let tile_y = (real_y & 7) as u16;

        // 確定圖塊數據的來源地址（LCDC位元4）
        let tile_data_base = if lcdc & 0x10 != 0 {
            0x8000 // 使用 8000h-8FFFh，圖塊編號為無符號
        } else {
            0x8800 // 使用 8800h-97FFh，圖塊編號為有符號
        };

        // 確定背景映射地址（LCDC位元3）
        let bg_map_base = if lcdc & 0x08 != 0 {
            0x9C00 // 使用 9C00h-9FFFh
        } else {
            0x9800 // 使用 9800h-9BFFh
        };

        // 渲染掃描線的每個像素
        for screen_x in 0..SCREEN_WIDTH {
            // 計算實際X座標（考慮捲動）
            let real_x = (screen_x as u16 + self.scroll_x as u16) & 0xFF;
            let tile_col = real_x >> 3;
            let tile_x = 7 - (real_x & 7); // 位元順序是從左到右

            // 從背景映射中獲取圖塊編號
            let map_addr = bg_map_base + tile_row * 32 + tile_col;
            let tile_num = mmu.read_byte(map_addr)?;

            // 計算圖塊數據的實際地址
            let tile_addr = if lcdc & 0x10 != 0 {
                tile_data_base + (tile_num as u16) * 16
            } else {
                tile_data_base + ((tile_num as i8 as i16 + 128) as u16) * 16
            };

            // 讀取圖塊數據
            let data_addr = tile_addr + tile_y * 2;
            let low_byte = mmu.read_byte(data_addr)?;
            let high_byte = mmu.read_byte(data_addr + 1)?;

            // 組合顏色編號
            let color_num = ((high_byte >> tile_x) & 1) << 1 | ((low_byte >> tile_x) & 1);

            // 應用背景調色盤
            let color_idx = (bgp >> (color_num * 2)) & 0x03;

            // 設置像素
            line_pixels[screen_x] = BgPixel::new(color_idx, color_num);
        }

        Ok(line_pixels)
    }
}
