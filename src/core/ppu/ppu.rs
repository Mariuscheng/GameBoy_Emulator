use crate::core::mmu::MMU;
use crate::utils::logger::log_to_file;
use std::{cell::RefCell, rc::Rc};

/// PPU (像素處理單元) 負責 Game Boy 的圖形渲染
pub struct PPU {
    pub mmu: Rc<RefCell<MMU>>,
    pub lcd_enabled: bool,
    pub framebuffer: Vec<u8>,
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
    pub vram: [u8; 0x2000],
}

impl PPU {
    pub fn new(mmu: Rc<RefCell<MMU>>) -> Self {
        Self {
            mmu,
            lcd_enabled: false,
            framebuffer: vec![0; 160 * 144],
            bgp: 0xFC,
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
            vram: [0; 0x2000],
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
        // 每次 tick 前同步 MMU 的 video_ram 到 PPU 的 vram
        let mmu_ref = self.mmu.borrow();
        self.vram.copy_from_slice(&mmu_ref.video_ram);
        self.dots += 1;
        // ...existing code...
        // 根據 VRAM palette 映射產生 framebuffer
        let w = 160;
        let h = 144;
        self.framebuffer.clear();
        for i in 0..(w * h) {
            let v = if i < self.vram.len() { self.vram[i] } else { 0 };
            let pix = match v {
                0x00 => 0,
                0x55 => 1,
                0xAA => 2,
                0xFF => 3,
                _ => 0,
            };
            // ...existing code...
            self.framebuffer.push(pix);
        }
    }
    /// 取得目前畫面緩衝區
    pub fn get_framebuffer(&self) -> &[u8] {
        if self.framebuffer.len() < 160 * 144 {
            static WHITE_FB: [u8; 160 * 144] = [0; 160 * 144];
            &WHITE_FB
        } else {
            &self.framebuffer
        }
    }
}
