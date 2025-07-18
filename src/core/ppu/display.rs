//! Display module, manages palette and framebuffer

use crate::core::error::Result;
use crate::interface::video::VideoInterface;
use std::any::Any;
use std::io;

use super::pixel::{BgPixel, Pixel};

#[derive(Debug)]
pub struct Display {
    frame: Vec<BgPixel>,
}

impl Display {
    pub fn new() -> Self {
        Self {
            frame: vec![BgPixel::new(0, 0); 160 * 144],
        }
    }

    pub fn clear(&mut self) {
        self.frame.fill(BgPixel::new(0, 0));
    }

    pub fn get_frame(&self) -> &[BgPixel] {
        &self.frame
    }

    pub fn get_frame_mut(&mut self) -> &mut [BgPixel] {
        &mut self.frame
    }

    pub fn convert_pixel_to_u32(pixel: &BgPixel) -> u32 {
        let [r, g, b, a] = pixel.rgba();
        ((a as u32) << 24) | ((r as u32) << 16) | ((g as u32) << 8) | (b as u32)
    }

    pub fn set_pixel(&mut self, x: usize, y: usize, pixel: BgPixel) -> Result<()> {
        if x >= 160 || y >= 144 {
            return Err(io::Error::new(
                io::ErrorKind::InvalidInput,
                "Pixel coordinates out of bounds",
            )
            .into());
        }

        self.frame[y * 160 + x] = pixel;
        Ok(())
    }

    pub fn write_line(&mut self, y: usize, line: &[BgPixel]) -> Result<()> {
        if y >= 144 || line.len() != 160 {
            return Err(
                io::Error::new(io::ErrorKind::InvalidInput, "Invalid line parameters").into(),
            );
        }

        let base_index = y * 160;
        for (x, &pixel) in line.iter().enumerate() {
            self.frame[base_index + x] = pixel;
        }

        Ok(())
    }

    pub fn get_buffer(&self) -> Vec<u8> {
        self.frame
            .iter()
            .flat_map(|pixel| {
                let rgba = pixel.rgba();
                vec![rgba[0], rgba[1], rgba[2], rgba[3]]
            })
            .collect()
    }

    pub fn render(&mut self, video: &mut Box<dyn VideoInterface>) -> Result<()> {
        video.update_frame(self.get_buffer());
        video
            .render()
            .map_err(|e| io::Error::new(io::ErrorKind::Other, e).into())
    }
}

impl VideoInterface for Display {
    fn update_frame(&mut self, frame_buffer: Vec<u8>) {
        let mut frame = Vec::with_capacity(frame_buffer.len() / 4);
        for chunk in frame_buffer.chunks_exact(4) {
            if let Ok(array) = <[u8; 4]>::try_from(chunk) {
                let [r, g, b, _] = array;
                let idx = ((r as u32 + g as u32 + b as u32) / 3) as u8 & 0x03;
                frame.push(BgPixel::new(idx, idx));
            }
        }
        self.frame = frame;
    }

    fn render(&mut self) -> std::result::Result<(), String> {
        Ok(())
    }

    fn resize(&mut self, _width: u32, _height: u32) -> std::result::Result<(), String> {
        Ok(())
    }

    fn as_any_mut(&mut self) -> &mut dyn Any {
        self
    }
}
