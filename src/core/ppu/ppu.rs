use super::sprite::SpriteRenderer;
use crate::Result;
use crate::mmu::MMU;
use std::cell::RefCell;
use std::io::Write;
use std::rc::Rc;
use std::time::Instant;

/// PPU狀態相關常量
const MODE_HBLANK: u8 = 0;
const MODE_VBLANK: u8 = 1;
const MODE_OAM_SCAN: u8 = 2;
const MODE_PIXEL_TRANSFER: u8 = 3;

/// LCD控制寄存器(LCDC)位元定義
const LCDC_LCD_ENABLE: u8 = 0x80;
const LCDC_WINDOW_TILEMAP: u8 = 0x40;
const LCDC_WINDOW_ENABLE: u8 = 0x20;
const LCDC_BG_TILE_DATA: u8 = 0x10;
const LCDC_BG_TILEMAP: u8 = 0x08;
const LCDC_OBJ_SIZE: u8 = 0x04;
const LCDC_OBJ_ENABLE: u8 = 0x02;
const LCDC_BG_ENABLE: u8 = 0x01;

/// PPU結構定義
pub struct PPU {
    /// MMU引用
    pub mmu: Rc<RefCell<MMU>>,
    /// 畫面緩衝區 (160x144 像素)
    pub framebuffer: Vec<u8>,
    /// 背景調色板 (FF47)
    pub bgp: u8,
    /// 精靈調色板0 (FF48)
    pub obp0: u8,
    /// 精靈調色板1 (FF49)
    pub obp1: u8,
    /// 背景水平捲動 (FF43)
    pub scx: u8,
    /// 背景垂直捲動 (FF42)
    pub scy: u8,
    /// 視窗X位置 (FF4B)
    pub wx: u8,
    /// 視窗Y位置 (FF4A)
    pub wy: u8,
    /// 物件屬性表
    pub oam: [u8; 160],
    /// 精靈渲染器
    pub sprite_renderer: SpriteRenderer,
    /// LCD控制寄存器 (FF40)
    pub lcdc: u8,
    /// 最後一幀時間戳
    pub last_frame_time: Instant,
    /// FPS計數器
    pub fps_counter: u32,
    /// PPU模式
    pub mode: u8,
    /// 當前掃描線 (FF44)
    pub ly: u8,
    /// 掃描線比較值 (FF45)
    pub lyc: u8,
    /// LCD狀態寄存器 (FF41)
    pub stat: u8,
    /// 點計數器
    pub dots: u32,
}

impl PPU {
    pub fn get_framebuffer(&self) -> &[u8] {
        &self.framebuffer
    }

    pub fn new(mmu: Rc<RefCell<MMU>>) -> Self {
        let mut file = std::fs::OpenOptions::new()
            .create(true)
            .append(true)
            .open("logs/debug.txt")
            .unwrap();
        writeln!(file, "[PPU-INIT] PPU初始化開始").unwrap();

        let ppu = Self {
            mmu,
            framebuffer: vec![255; 160 * 144], // 初始化為白色
            bgp: 0xFC,                         // 標準 GameBoy 調色盤 (11111100)
            obp0: 0xFF,
            obp1: 0xFF,
            scx: 0,
            scy: 0,
            wx: 0,
            wy: 0,
            oam: [0; 160],
            sprite_renderer: SpriteRenderer::new(),
            lcdc: 0x80, // 只啟用LCD，其他功能關閉
            last_frame_time: Instant::now(),
            fps_counter: 0,
            mode: 2, // 開始於OAM掃描模式
            ly: 0,
            lyc: 0,
            stat: 0,
            dots: 0,
        };

        writeln!(file, "[PPU-INIT] PPU初始化完成").unwrap();
        ppu
    }

    pub fn reset(&mut self) {
        let mut file = std::fs::OpenOptions::new()
            .create(true)
            .append(true)
            .open("logs/debug.txt")
            .unwrap();
        writeln!(file, "[PPU-RESET] PPU重置開始").unwrap();

        self.mode = 2;
        self.ly = 0;
        self.dots = 0;
        self.framebuffer.fill(255); // 重置為白色

        writeln!(file, "[PPU-RESET] PPU重置完成").unwrap();
    }

    pub fn step(&mut self, cycles: u32) -> Result<()> {
        self.dots += cycles;

        match self.mode {
            2 => {
                // OAM Scan Mode (80 dots)
                if self.dots >= 80 {
                    self.dots -= 80;
                    self.mode = 3;
                }
            }
            3 => {
                // Pixel Transfer Mode (172 dots)
                if self.dots >= 172 {
                    self.dots -= 172;
                    self.mode = 0;
                    self.render_scanline()?;
                }
            }
            0 => {
                // H-Blank Mode (204 dots)
                if self.dots >= 204 {
                    self.dots -= 204;
                    self.ly += 1;

                    if self.ly == 144 {
                        self.mode = 1;
                        // Set V-Blank interrupt
                        let mut mmu = self.mmu.borrow_mut();
                        let if_reg = mmu.read_byte(0xFF0F)?;
                        mmu.write_byte(0xFF0F, if_reg | 0x01)?; // Set V-Blank interrupt (bit 0)
                    } else {
                        self.mode = 2;
                    }
                }
            }
            1 => {
                // V-Blank Mode (4560 dots total, 10 scanlines)
                if self.dots >= 456 {
                    self.dots -= 456;
                    self.ly += 1;

                    if self.ly > 153 {
                        self.ly = 0;
                        self.mode = 2;
                    }
                }
            }
            _ => unreachable!(),
        }

        Ok(())
    }

    pub fn render_scanline(&mut self) -> Result<()> {
        // 如果LCD禁用，填充白色
        if self.lcdc & LCDC_LCD_ENABLE == 0 {
            for i in 0..160 {
                let offset = (self.ly as usize * 160 + i) as usize;
                if offset < self.framebuffer.len() {
                    self.framebuffer[offset] = 255; // 填充白色
                }
            }
            return Ok(());
        }

        // 簡單測試：填充純色來檢查是否能正常顯示
        for i in 0..160 {
            let offset = (self.ly as usize * 160 + i) as usize;
            if offset < self.framebuffer.len() {
                // 創建簡單的測試圖案
                if (i / 8) % 2 == 0 {
                    self.framebuffer[offset] = 255; // 白色
                } else {
                    self.framebuffer[offset] = 128; // 灰色
                }
            }
        }

        Ok(())
    }

    pub fn render_background(&mut self) -> Result<()> {
        // 暫時留空，稍後實現
        Ok(())
    }

    pub fn render_window(&mut self) -> Result<()> {
        // 暫時留空，稍後實現
        Ok(())
    }

    pub fn render_sprites(&mut self) -> Result<()> {
        // 暫時留空，稍後實現
        Ok(())
    }

    fn update_background(&mut self, mmu: &MMU) -> Result<()> {
        Ok(())
    }

    fn update_window(&mut self, mmu: &MMU) -> Result<()> {
        Ok(())
    }

    fn update_sprites(&mut self, mmu: &MMU) -> Result<()> {
        Ok(())
    }
}
