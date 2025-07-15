/// DummyVideoInterface: Headless video 實作，僅供 RL/自動化測試用
#[derive(Debug)]
pub struct DummyVideoInterface;

// use std::any::Any; // 已在檔案前方引入，移除重複

impl crate::interface::video::VideoInterface for DummyVideoInterface {
    fn update_frame(&mut self, _frame_buffer: Vec<u8>) {
        // 不做任何事
    }
    fn render(&mut self) -> Result<(), crate::error::Error> {
        Ok(())
    }
    fn resize(&mut self, _width: u32, _height: u32) -> Result<(), crate::error::Error> {
        Ok(())
    }
    fn as_any_mut(&mut self) -> &mut dyn Any {
        self
    }
}
// DummyPixels 及 unsafe block 已移除，headless 模式僅用 DummyVideoInterface。
use pixels::Pixels;
use std::any::Any;
use std::fmt::{self, Debug};
use winit::window::Window;

pub trait VideoInterface: Debug + Any {
    fn update_frame(&mut self, frame_buffer: Vec<u8>);
    fn render(&mut self) -> Result<(), crate::error::Error>;
    fn resize(&mut self, new_width: u32, new_height: u32) -> Result<(), crate::error::Error>;
    fn as_any_mut(&mut self) -> &mut dyn Any;
}

pub struct PixelsDisplay {
    pixels: Pixels<'static>,
}

impl PixelsDisplay {
    pub fn new(window: &'static Window) -> Result<Self, pixels::Error> {
        // Get actual window size
        let window_size = window.inner_size();

        // SurfaceTexture uses the entire window size
        let surface_texture =
            pixels::SurfaceTexture::new(window_size.width, window_size.height, window);

        // Pixels internally uses Game Boy's native resolution
        let pixels = Pixels::new(160, 144, surface_texture)?;
        Ok(Self { pixels })
    }

    pub fn resize(
        &mut self,
        new_width: u32,
        new_height: u32,
        _window: &Window,
    ) -> Result<(), Box<dyn std::error::Error>> {
        self.pixels
            .resize_surface(new_width, new_height)
            .map_err(|e| Box::new(e) as Box<dyn std::error::Error>)
    }
}

impl Debug for PixelsDisplay {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.debug_struct("PixelsDisplay")
            .field("pixels", &"Pixels { ... }")
            .finish()
    }
}

impl VideoInterface for PixelsDisplay {
    fn update_frame(&mut self, frame_buffer: Vec<u8>) {
        if frame_buffer.len() >= 160 * 144 * 4 {
            // Copy the entire frame buffer
            self.pixels
                .frame_mut()
                .copy_from_slice(&frame_buffer[..160 * 144 * 4]);
        } else {
            // If the buffer is too small, fill with white
            for pixel in self.pixels.frame_mut().chunks_exact_mut(4) {
                pixel.copy_from_slice(&[0xFF, 0xFF, 0xFF, 0xFF]);
            }
            // Log the error
            eprintln!(
                "Warning: Frame buffer size mismatch. Expected {} bytes, got {}",
                160 * 144 * 4,
                frame_buffer.len()
            );
        }
    }

    fn render(&mut self) -> Result<(), crate::error::Error> {
        self.pixels
            .render()
            .map_err(|e| crate::error::Error::Video(e.to_string()))
    }

    fn resize(&mut self, new_width: u32, new_height: u32) -> Result<(), crate::error::Error> {
        self.pixels
            .resize_surface(new_width, new_height)
            .map_err(|e| crate::error::Error::Video(e.to_string()))
    }

    fn as_any_mut(&mut self) -> &mut dyn Any {
        self
    }
}
