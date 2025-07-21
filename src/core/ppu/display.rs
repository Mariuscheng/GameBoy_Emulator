use std::result::Result;
/// 顯示模組，管理 palette 與 framebuffer

#[derive(Debug)]
pub struct Display {
    framebuffer: Vec<u32>,
    // [LOG] 建議在每次 fill/更新時插入：
    // log_to_file!("[Display] framebuffer filled, size={}", self.framebuffer.len());
    color_map: Vec<u32>,
}

impl Display {
    pub fn new() -> Self {
        let color_map = vec![
            0xFFFFFFFF, // White (0)
            0xFFAAAAAA, // Light gray (1)
            0xFF555555, // Dark gray (2)
            0xFF000000, // Black (3)
        ];
        Self {
            framebuffer: vec![0xFFFFFFFF; 160 * 144],
            color_map,
        }
    }
    pub fn clear(&mut self) {
        self.framebuffer.fill(0xFFFFFFFF);
    }
    pub fn render(
        &mut self,
        video: &mut dyn crate::interface::video::VideoInterface,
    ) -> crate::core::error::Result<()> {
        video.render().map_err(|_| {
            crate::core::error::Error::Hardware(crate::core::error::HardwareError::PPU(
                "Rendering failed".to_string(),
            ))
        })?;
        Ok(())
    }
    pub fn get_frame(&self) -> &[u32] {
        &self.framebuffer
    }
}

pub fn render_scanline(ppu: &mut super::ppu::PPU) {
    // TODO: 根據 LY, SCY, SCX, LCDC 等狀態渲染一條掃描線
}
