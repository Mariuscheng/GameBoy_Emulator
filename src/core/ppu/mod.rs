// PPU 核心模組
mod background;
mod display;
mod lcd;
mod pixel;
mod registers;
mod sprite;
mod tile;
mod types;
mod window;

use std::cell::RefCell;
use std::rc::Rc;

use crate::core::error::{Error, Result};
use crate::core::mmu::MMU;
use crate::interface::video::VideoInterface;

// 內部使用的類型
use self::background::BackgroundRenderer;
use self::display::Display;
use self::sprite::SpriteRenderer;
use self::window::WindowRenderer;

// 常量定義
const SCREEN_WIDTH: usize = 160;
const SCREEN_HEIGHT: usize = 144;
const VBLANK_LINE: u8 = 144;
const MAX_LINE: u8 = 153;

#[derive(Debug)]
pub struct PPU {
    pub background: BackgroundRenderer,
    pub window: WindowRenderer,
    pub sprites: SpriteRenderer,
    pub display: Display,
    mmu: Rc<RefCell<MMU>>,
    video: Box<dyn VideoInterface>,

    // PPU state
    mode_clock: u32,
    current_line: u8,
    current_mode: u8,
}

impl PPU {
    pub fn new(mmu: Rc<RefCell<MMU>>, video: Box<dyn VideoInterface>) -> Self {
        Self {
            background: BackgroundRenderer::new(),
            window: WindowRenderer::new(),
            sprites: SpriteRenderer::new(),
            display: Display::new(),
            mmu,
            video,
            mode_clock: 0,
            current_line: 0,
            current_mode: 0,
        }
    }

    pub fn render(&mut self) -> crate::core::error::Result<()> {
        // 自動補上：每一 frame 觸發背景渲染並寫入 display framebuffer
        for line in 0..144 {
            // 背景渲染，取得每一行像素
            let bg_line = self.background.render_line(line, &self.mmu.borrow())?;
            // 寫入 display framebuffer
            self.display.write_line(line as usize, &bg_line);
        }
        // 只保留最重要 debug 訊息
        let fb = self.display.get_buffer();
        log::debug!("[DEBUG][PPU] framebuffer[0..8]: {:?}", &fb[0..8]);
        self.video.update_frame(self.display.get_buffer());
        self.video.render().map_err(|e| Error::Video(e))
    }

    pub fn render_line(&mut self) -> Result<()> {
        let current_line = self.current_line;
        let mmu = self.mmu.borrow();

        // 更新精靈狀態
        self.sprites.update_sprites(&mmu)?;

        // 獲取背景層
        let bg_line = self.background.render_line(current_line, &mmu)?;

        // 獲取精靈層
        let sprite_line = self.sprites.render_line(current_line, &bg_line, &mmu)?;

        // 混合背景和精靈層
        let base_index = current_line as usize * SCREEN_WIDTH;
        for x in 0..SCREEN_WIDTH {
            // 如果有精靈像素，使用精靈像素，否則使用背景像素
            self.display.get_frame_mut()[base_index + x] = sprite_line[x].unwrap_or(bg_line[x]);
        }

        Ok(())
    }

    pub fn get_video_mut(&mut self) -> &mut dyn VideoInterface {
        self.video.as_mut()
    }
    pub fn step(&mut self, cycles: u32) -> Result<()> {
        // Check if LCD is enabled
        let lcd_enabled = {
            let mmu = self.mmu.borrow();
            let lcdc = mmu.read_byte(0xFF40).unwrap_or(0);
            (lcdc & 0x80) != 0
        };

        if !lcd_enabled {
            // When LCD is disabled
            self.display.clear();
            self.current_mode = 0;
            self.current_line = 0;
            self.mode_clock = 0;
            self.update_lcd_status()?;
            // 寫入 LY/STAT 到 MMU
            let mut mmu = self.mmu.borrow_mut();
            mmu.ly = 0;
            mmu.lcd_registers.stat = (mmu.lcd_registers.stat & 0xFC) | (self.current_mode & 0x03);
            return Ok(());
        }

        self.mode_clock += cycles;
        let old_mode = self.current_mode;
        let old_line = self.current_line;

        match self.current_mode {
            0 => {
                // H-Blank (204 cycles)
                if self.mode_clock >= 204 {
                    self.mode_clock = 0;
                    self.current_line += 1;

                    if self.current_line == VBLANK_LINE {
                        // Enter V-Blank
                        self.current_mode = 1;
                        let mut mmu = self.mmu.borrow_mut();
                        mmu.interrupt_flags |= 1 << 0; // Set VBlank interrupt
                        drop(mmu);

                        // Frame rendering complete, update display
                        self.vblank()?;
                    } else {
                        // Return to OAM scan
                        self.current_mode = 2;
                    }
                }
            }
            1 => {
                // V-Blank (4560 cycles total, 10 lines * 456)
                if self.mode_clock >= 456 {
                    self.mode_clock = 0;
                    self.current_line += 1;

                    if self.current_line > MAX_LINE {
                        // V-Blank ends, return to first line
                        self.current_mode = 2;
                        self.current_line = 0;
                    }
                }
            }
            2 => {
                // OAM scan (80 cycles)
                if self.mode_clock >= 80 {
                    self.mode_clock = 0;
                    self.current_mode = 3;
                }
            }
            3 => {
                // Transfer data to LCD (172 cycles)
                if self.mode_clock >= 172 {
                    self.mode_clock = 0;
                    self.current_mode = 0; // Enter H-Blank

                    // Render current line
                    if self.current_line < SCREEN_HEIGHT as u8 {
                        self.render_line()?;
                    }
                }
            }
            _ => unreachable!(),
        }

        // 每次掃描線/模式變動時，寫入 LY/STAT 到 MMU
        if old_mode != self.current_mode || old_line != self.current_line {
            let mut mmu = self.mmu.borrow_mut();
            mmu.ly = self.current_line;
            mmu.lcd_registers.stat = (mmu.lcd_registers.stat & 0xFC) | (self.current_mode & 0x03);
        }

        // 仍需呼叫 update_lcd_status 以維持 LYC=LY 比對與 STAT bit2
        if old_mode != self.current_mode {
            self.update_lcd_status()?;
        }

        Ok(())
    }

    pub fn update(&mut self, cycles: u32) -> Result<()> {
        self.step(cycles)
    }

    pub fn reset(&mut self) -> Result<()> {
        self.mode_clock = 0;
        self.current_line = 0;
        self.current_mode = 0;
        self.display.clear();
        Ok(())
    }

    pub fn get_line(&self) -> u8 {
        self.current_line
    }

    pub fn get_mode(&self) -> u8 {
        self.current_mode
    }
    #[allow(dead_code)]
    fn draw_line(&mut self, line: u8, mmu: &MMU) -> Result<()> {
        let bg_line = self.background.render_line(line, mmu)?;

        let base_index = line as usize * SCREEN_WIDTH;
        for x in 0..SCREEN_WIDTH {
            self.display.get_frame_mut()[base_index + x] = bg_line[x];
        }
        Ok(())
    }

    fn render_current_frame(&mut self) -> Result<()> {
        // Clear entire screen
        self.display.clear();

        // Render each line
        for line in 0..SCREEN_HEIGHT {
            let line = line as u8;

            // First get background line data
            let bg_line = {
                let mmu = self.mmu.borrow();
                self.background.render_line(line, &mmu)?
            };

            // Then update display buffer
            let base_index = line as usize * SCREEN_WIDTH;
            for x in 0..SCREEN_WIDTH {
                self.display.get_frame_mut()[base_index + x] = bg_line[x];
            }
        }

        Ok(())
    }

    fn vblank(&mut self) -> Result<()> {
        // Update screen during V-Blank
        self.render_current_frame()?;
        self.display.render(&mut self.video)
    }

    fn update_lcd_status(&mut self) -> Result<()> {
        let mut mmu = self.mmu.borrow_mut();
        let mut stat = mmu.read_byte(0xFF41)?;

        // Clear current mode bits (0-1)
        stat &= 0xFC;
        // Set new mode
        stat |= self.current_mode & 0x03;

        // Update LYC=LY comparison flag (bit 2)
        let lyc = mmu.read_byte(0xFF45)?;
        if self.current_line == lyc {
            stat |= 0x04;
        } else {
            stat &= !0x04;
        }

        // Write updated STAT
        mmu.write_byte(0xFF41, stat)?;

        // Update LY register
        mmu.write_byte(0xFF44, self.current_line)?;

        Ok(())
    }
}

#[cfg(test)]
mod tests {
    #[test]
    fn test_ppu_initialization() {
        // TODO: Implement tests
    }
}
