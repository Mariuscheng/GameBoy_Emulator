//! 精靈渲染器，產生精靈圖層像素

use super::pixel::BgPixel;
use crate::core::error::Result;
use crate::core::mmu::MMU;

const SPRITES_PER_LINE: usize = 10;
const SPRITE_HEIGHT: u8 = 8;
const SPRITE_WIDTH: u8 = 8;
const SCREEN_WIDTH: usize = 160;

/// 精靈屬性常量
const SPRITE_PRIORITY_FLAG: u8 = 0x80; // Bit 7: BG優先級
const SPRITE_FLIP_Y_FLAG: u8 = 0x40; // Bit 6: Y軸翻轉
const SPRITE_FLIP_X_FLAG: u8 = 0x20; // Bit 5: X軸翻轉
const SPRITE_PALETTE_FLAG: u8 = 0x10; // Bit 4: 調色盤選擇

#[derive(Debug, Clone)]
pub struct Sprite {
    /// X座標，經過-8調整後的實際螢幕位置
    x: i16,

    /// Y座標，經過-16調整後的實際螢幕位置
    y: i16,

    /// 圖塊索引，指向VRAM中的圖塊數據
    tile_index: u8,

    /// 屬性標誌
    /// - Bit 7: BG優先級 (1=BG優先，0=精靈優先)
    /// - Bit 6: Y軸翻轉 (1=翻轉)
    /// - Bit 5: X軸翻轉 (1=翻轉)
    /// - Bit 4: 調色盤選擇 (0=OBP0，1=OBP1)
    flags: u8,
}

impl Sprite {
    fn new(y: u8, x: u8, tile_index: u8, flags: u8) -> Self {
        Self {
            x: x as i16 - 8,  // 調整為實際螢幕座標
            y: y as i16 - 16, // 調整為實際螢幕座標
            tile_index,
            flags,
        }
    }

    fn is_visible(&self, line: u8) -> bool {
        let y = line as i16;
        self.y <= y && y < self.y + SPRITE_HEIGHT as i16
    }

    /// 檢查精靈是否具有背景優先級
    /// 如果為true，則當背景像素非透明時，精靈將被隱藏在背景之後
    fn priority(&self) -> bool {
        self.flags & SPRITE_PRIORITY_FLAG != 0
    }

    /// 檢查精靈是否在X軸上翻轉
    fn flip_x(&self) -> bool {
        self.flags & SPRITE_FLIP_X_FLAG != 0
    }

    /// 檢查精靈是否在Y軸上翻轉
    fn flip_y(&self) -> bool {
        self.flags & SPRITE_FLIP_Y_FLAG != 0
    }

    /// 獲取精靈使用的調色盤
    /// 返回true表示使用OBP1，false表示使用OBP0
    fn palette(&self) -> bool {
        self.flags & SPRITE_PALETTE_FLAG != 0
    }
}

#[derive(Debug)]
pub struct SpriteRenderer {
    sprites: Vec<Sprite>,
}

impl SpriteRenderer {
    pub fn new() -> Self {
        Self {
            sprites: Vec::new(),
        }
    }

    pub fn update_sprites(&mut self, mmu: &MMU) -> Result<()> {
        self.sprites.clear();

        // 從 OAM 讀取精靈資料
        for i in 0..40 {
            let base = 0xFE00 + i * 4;
            let y = mmu.read_byte(base)?;
            let x = mmu.read_byte(base + 1)?;
            let tile = mmu.read_byte(base + 2)?;
            let flags = mmu.read_byte(base + 3)?;

            self.sprites.push(Sprite::new(y, x, tile, flags));
        }

        // 根據 X 座標排序精靈（X 座標小的優先）
        self.sprites.sort_by_key(|s| s.x);

        Ok(())
    }

    pub fn render_line(
        &self,
        line: u8,
        bg_line: &[BgPixel],
        mmu: &MMU,
    ) -> Result<Vec<Option<BgPixel>>> {
        let mut sprite_line = vec![None; SCREEN_WIDTH];
        let mut sprites_on_line = 0;

        // 反向遍歷精靈（最後的精靈先繪製，這樣優先級高的精靈會覆蓋優先級低的）
        for sprite in self.sprites.iter().rev() {
            if !sprite.is_visible(line) || sprites_on_line >= SPRITES_PER_LINE {
                continue;
            }

            sprites_on_line += 1;

            // 計算精靈中的 Y 偏移
            let mut y_offset = (line as i16 - sprite.y) as u8;
            if sprite.flip_y() {
                y_offset = SPRITE_HEIGHT - 1 - y_offset;
            }

            // 獲取精靈圖塊數據
            let tile_addr = 0x8000 + (sprite.tile_index as u16 * 16);
            let low_byte = mmu.read_byte(tile_addr + (y_offset as u16 * 2))?;
            let high_byte = mmu.read_byte(tile_addr + (y_offset as u16 * 2) + 1)?;

            // 選擇調色盤
            let palette_addr = if sprite.palette() { 0xFF49 } else { 0xFF48 };
            let palette = mmu.read_byte(palette_addr)?;

            // 遍歷精靈的每個像素
            for x in 0..SPRITE_WIDTH {
                let screen_x = sprite.x as i16 + x as i16;
                if screen_x < 0 || screen_x >= SCREEN_WIDTH as i16 {
                    continue;
                }

                let bit_index = if sprite.flip_x() {
                    x
                } else {
                    SPRITE_WIDTH - 1 - x
                };

                // 獲取像素顏色編號（0-3）
                let color_num = ((high_byte >> bit_index) & 1) << 1 | ((low_byte >> bit_index) & 1);

                // 如果顏色為 0，則為透明
                if color_num == 0 {
                    continue;
                }

                // 獲取實際顏色
                let color_idx = (palette >> (color_num * 2)) & 0x03;

                // 檢查背景優先級
                let screen_x = screen_x as usize;
                if sprite.priority() && bg_line[screen_x].color_number() != 0 {
                    continue;
                }

                sprite_line[screen_x] = Some(BgPixel::new(color_idx, color_num));
            }
        }

        Ok(sprite_line)
    }
}
