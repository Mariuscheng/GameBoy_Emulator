extern crate sdl3;

use sdl3::event::Event;
use sdl3::keyboard::Keycode;
use sdl3::pixels::Color;
use std::time::Duration;

use crate::core::emulator::Emulator;
use crate::interface::mock_display::DisplayBackend;
// use crate::core::utils::framebuffer_api::indices_to_sdl_colors; // 若有需要請補 core::utils

pub struct SdlDisplayBackend {
    canvas: sdl3::render::Canvas<sdl3::video::Window>,
}

impl SdlDisplayBackend {
    /// 直接公開 draw_frame 方法，供 main.rs 呼叫
    pub fn draw_frame(&mut self, framebuffer: &[(u8, u8, u8)]) {
        self.canvas.set_draw_color(Color::RGB(255, 255, 255));
        self.canvas.clear();
        let colors = Self::indices_to_sdl_colors(framebuffer);

        // Debug: log 前 32 個 framebuffer index 與 RGB
        use std::fs::OpenOptions;
        use std::io::Write;
        if !framebuffer.is_empty() {
            let mut log = OpenOptions::new()
                .create(true)
                .append(true)
                .open("logs/emulator.log")
                .unwrap();
            writeln!(log, "[draw_frame] 前 32 個 framebuffer index 與 RGB:").ok();
            for i in 0..32.min(framebuffer.len()) {
                let idx = framebuffer[i];
                let color = &colors[i];
                writeln!(
                    log,
                    "idx[{}]={:?}  RGB=({}, {}, {})",
                    i, idx, color.r, color.g, color.b
                )
                .ok();
            }
        }
        let width = 160;
        let height = 144;
        // 每次直接建立 Texture，延長 texture_creator 生命週期
        let texture_creator = self.canvas.texture_creator();
        let mut texture = texture_creator
            .create_texture_streaming(None, width, height)
            .unwrap();
        // 準備 RGB buffer
        // 測試：全部設為紅色 (255,0,0)
        let mut rgb_buf = vec![0u8; width as usize * height as usize * 3];
        for i in 0..(width as usize * height as usize) {
            rgb_buf[i * 3] = 255;
            rgb_buf[i * 3 + 1] = 0;
            rgb_buf[i * 3 + 2] = 0;
        }
        texture.update(None, &rgb_buf, width as usize * 3).unwrap();
        self.canvas.clear();
        let dst_rect = sdl3::rect::Rect::new(0, 0, width * 2, height * 2);
        let _ = self.canvas.copy(&texture, None, dst_rect);
        self.canvas.present();
    }

    /// 將 Game Boy framebuffer RGB tuple 直接轉換為 SDL 顏色陣列
    fn indices_to_sdl_colors(framebuffer: &[(u8, u8, u8)]) -> Vec<sdl3::pixels::Color> {
        framebuffer
            .iter()
            .map(|&(r, g, b)| sdl3::pixels::Color::RGB(r, g, b))
            .collect()
    }
    /// 取得目前 Joypad 狀態（鍵盤映射）
    pub fn get_joypad_state(
        event_pump: &sdl3::EventPump,
    ) -> crate::interface::input::joypad::JoypadState {
        let keyboard_state = event_pump.keyboard_state();
        use crate::interface::input::joypad::JoypadState;
        JoypadState {
            start: keyboard_state.is_scancode_pressed(sdl3::keyboard::Scancode::Return),
            select: keyboard_state.is_scancode_pressed(sdl3::keyboard::Scancode::Backspace),
            b: keyboard_state.is_scancode_pressed(sdl3::keyboard::Scancode::X),
            a: keyboard_state.is_scancode_pressed(sdl3::keyboard::Scancode::Z),
            down: keyboard_state.is_scancode_pressed(sdl3::keyboard::Scancode::Down),
            up: keyboard_state.is_scancode_pressed(sdl3::keyboard::Scancode::Up),
            left: keyboard_state.is_scancode_pressed(sdl3::keyboard::Scancode::Left),
            right: keyboard_state.is_scancode_pressed(sdl3::keyboard::Scancode::Right),
        }
    }
    pub fn new() -> (Self, sdl3::EventPump) {
        let sdl_context = sdl3::init().unwrap();
        let video_subsystem = sdl_context.video().unwrap();
        let window = video_subsystem
            .window("GameBoy模擬器", 320, 288)
            .position_centered()
            .build()
            .unwrap();
        let mut canvas = window.into_canvas();
        let event_pump = sdl_context.event_pump().unwrap();
        (Self { canvas }, event_pump)
    }
}

impl DisplayBackend for SdlDisplayBackend {
    fn poll_event(&mut self) -> bool {
        // SDL3 event loop 由 main.rs 控制，這裡直接回傳 false
        false
    }
    fn draw_frame(&mut self, framebuffer: &[(u8, u8, u8)]) {
        SdlDisplayBackend::draw_frame(self, framebuffer);
    }
}

// 通用主迴圈，與 mock_display.rs 相同
pub fn run_emulator_with_display<B: DisplayBackend>(emulator: &mut Emulator, backend: &mut B) {
    crate::core::utils::logger::log_to_file("[EMU_LOOP] Enter main emu loop");
    loop {
        emulator.step();
        backend.draw_frame(emulator.ppu.get_framebuffer());
        if backend.poll_event() {
            break;
        }
    }
}

// 舊 API 包裝，方便 main.rs 不用大改
pub fn init_sdl(emulator: &mut Emulator) {
    let (mut backend, _event_pump) = SdlDisplayBackend::new();
    run_emulator_with_display(emulator, &mut backend);
}
