// MockDisplayBackend: 用於自動化測試，不需 SDL 視窗
use crate::core::emulator::Emulator;

pub trait DisplayBackend {
    fn draw_frame(&mut self, framebuffer: &[(u8, u8, u8)]);
    fn poll_event(&mut self) -> bool; // 回傳 true 則結束主迴圈
}

pub struct MockDisplayBackend {
    pub frames: Vec<Vec<(u8, u8, u8)>>,
    pub quit_after: usize,
    pub calls: usize,
}

impl MockDisplayBackend {
    pub fn new(quit_after: usize) -> Self {
        Self {
            frames: vec![],
            quit_after,
            calls: 0,
        }
    }
}

impl DisplayBackend for MockDisplayBackend {
    fn draw_frame(&mut self, framebuffer: &[(u8, u8, u8)]) {
        self.frames.push(framebuffer.to_vec());
        self.calls += 1;
    }
    fn poll_event(&mut self) -> bool {
        self.calls >= self.quit_after
    }
}

// 通用主迴圈，可注入任何 DisplayBackend
pub fn run_emulator_with_display<B: DisplayBackend>(emulator: &mut Emulator, backend: &mut B) {
    loop {
        emulator.step();
        backend.draw_frame(&emulator.ppu.framebuffer);
        if backend.poll_event() {
            break;
        }
    }
}
