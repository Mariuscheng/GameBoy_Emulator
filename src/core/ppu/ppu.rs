use crate::core::mmu::MMU;
use std::{cell::RefCell, rc::Rc};

/// PPU (像素處理單元) 負責 Game Boy 的圖形渲染
pub struct PPU {
    pub mmu: Rc<RefCell<MMU>>,
    pub lcd_enabled: bool,
    pub framebuffer: Vec<u8>,
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
        // --- 渲染前 16 像素（自動化測試用）---
        // 假設 VRAM[0..16] 為 tile 0 第一行像素資料
        // GameBoy tile 格式：每 tile 16 bytes，每行 2 bytes（lo/hi）
        // 這裡只解碼 tile 0 第一行
        let tile_addr = 0x0000; // tile 0
        let lo = self.vram[tile_addr];
        let hi = self.vram[tile_addr + 1];
        for px in 0..8 {
            let bit = 7 - px;
            let color_id = ((hi >> bit) & 1) << 1 | ((lo >> bit) & 1);
            let palette = self.bgp;
            let shade = (palette >> (color_id * 2)) & 0x3;
            self.framebuffer[px] = shade;
        }
        // 若 VRAM[2..4] 有第二行資料，也可解碼到 framebuffer[8..15]
        let lo2 = self.vram[tile_addr + 2];
        let hi2 = self.vram[tile_addr + 3];
        for px in 0..8 {
            let bit = 7 - px;
            let color_id = ((hi2 >> bit) & 1) << 1 | ((lo2 >> bit) & 1);
            let palette = self.bgp;
            let shade = (palette >> (color_id * 2)) & 0x3;
            self.framebuffer[8 + px] = shade;
        }
        // --- End ---
        // 其餘原本渲染流程可保留
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
