use sdl3::event::Event;
use sdl3::joystick::Joystick;
use sdl3::pixels::Color;
use sdl3::render::Canvas;
use sdl3::video::Window;
use std::any::Any;
use std::fmt;

use crate::interface::video::VideoInterface;

const GAMEBOY_WIDTH: u32 = 160;
const GAMEBOY_HEIGHT: u32 = 144;
const SCALE_FACTOR: u32 = 3;

#[derive(Debug, Clone, Copy)]
pub enum GBColor {
    White = 0,
    LightGray = 1,
    DarkGray = 2,
    Black = 3,
}

impl GBColor {
    // 將 2-bit 值轉換為 GBColor
    pub fn from_2bit(value: u8) -> Self {
        match value & 0b11 {
            0 => GBColor::White,
            1 => GBColor::LightGray,
            2 => GBColor::DarkGray,
            3 => GBColor::Black,
            _ => unreachable!(),
        }
    }

    // 轉換為 RGB 顏色
    pub fn to_rgb(self) -> Color {
        match self {
            GBColor::White => Color::RGB(255, 255, 255),     // 最亮
            GBColor::LightGray => Color::RGB(192, 192, 192), // 亮灰
            GBColor::DarkGray => Color::RGB(96, 96, 96),     // 暗灰
            GBColor::Black => Color::RGB(0, 0, 0),           // 最暗
        }
    }
}

pub struct Display {
    canvas: Canvas<Window>,
    buffer: Vec<GBColor>,
    joystick: Option<Joystick>,
}

impl fmt::Debug for Display {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.debug_struct("Display")
            .field("buffer", &self.buffer)
            .finish()
    }
}

impl Display {
    pub fn new(sdl_context: &sdl3::Sdl) -> Result<Display, String> {
        let video_subsystem = sdl_context.video().map_err(|e| e.to_string())?;
        let joystick_subsystem = sdl_context.joystick().map_err(|e| e.to_string())?;

        // 檢查是否有可用的搖桿
        let mut joystick = None;
        for id in 0..4 {
            // 檢查前 4 個搖桿插槽
            if let Ok(stick) = joystick_subsystem.open(id) {
                println!("找到搖桿：{}", stick.name());
                joystick = Some(stick);
                break;
            }
        }

        let window = video_subsystem
            .window(
                "GameBoy 模擬器",
                GAMEBOY_WIDTH * SCALE_FACTOR,
                GAMEBOY_HEIGHT * SCALE_FACTOR,
            )
            .position_centered()
            .build()
            .map_err(|e| e.to_string())?;

        let canvas = window.into_canvas();

        Ok(Display {
            canvas,
            buffer: vec![GBColor::White; (GAMEBOY_WIDTH * GAMEBOY_HEIGHT) as usize],
            joystick,
        })
    }

    pub fn set_scanline(&mut self, y: u32, data: &[u8]) {
        if y < GAMEBOY_HEIGHT && data.len() >= GAMEBOY_WIDTH as usize {
            for x in 0..GAMEBOY_WIDTH {
                let color = GBColor::from_2bit(data[x as usize]);
                self.set_pixel(x, y, color);
            }
        }
    }

    // 設置單個像素的顏色
    pub fn set_pixel(&mut self, x: u32, y: u32, color: GBColor) {
        if x < GAMEBOY_WIDTH && y < GAMEBOY_HEIGHT {
            let index = (y * GAMEBOY_WIDTH + x) as usize;
            self.buffer[index] = color;
        }
    }

    // 渲染整個畫面
    pub fn render(&mut self) -> Result<(), String> {
        self.canvas.clear();

        // 遍歷所有像素並繪製
        for y in 0..GAMEBOY_HEIGHT {
            for x in 0..GAMEBOY_WIDTH {
                let index = (y * GAMEBOY_WIDTH + x) as usize;
                let color = self.buffer[index].to_rgb();
                self.canvas.set_draw_color(color);

                // 繪製放大後的像素
                let rect = sdl3::rect::Rect::new(
                    (x * SCALE_FACTOR) as i32,
                    (y * SCALE_FACTOR) as i32,
                    SCALE_FACTOR,
                    SCALE_FACTOR,
                );
                self.canvas.fill_rect(rect).map_err(|e| e.to_string())?;
            }
        }

        self.canvas.present();
        Ok(())
    }

    pub fn handle_joystick_event(&mut self, event: Event) {
        match event {
            Event::JoyDeviceRemoved { which: _, .. } => {
                println!("搖桿已斷開連接");
                self.joystick = None;
            }
            _ => {}
        }
    }
}

impl VideoInterface for Display {
    fn update_frame(&mut self, frame_buffer: Vec<u8>) {
        // 將接收到的幀緩衝區轉換為 GBColor
        for (i, &value) in frame_buffer.iter().enumerate() {
            if i < self.buffer.len() {
                self.buffer[i] = GBColor::from_2bit(value);
            }
        }
    }

    fn render(&mut self) -> Result<(), String> {
        self.render()
    }

    fn resize(&mut self, _new_width: u32, _new_height: u32) -> Result<(), String> {
        // GameBoy 的解析度是固定的，所以我們不需要實現resize
        Ok(())
    }

    fn as_any_mut(&mut self) -> &mut dyn Any {
        self
    }
}
