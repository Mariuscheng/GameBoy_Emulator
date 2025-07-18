/// DummyVideoInterface: Headless video 實作，僅供 RL/自動化測試用
#[derive(Debug)]
pub struct DummyVideoInterface;

// use std::any::Any; // 已在檔案前方引入，移除重複

impl crate::interface::video::VideoInterface for DummyVideoInterface {
    fn update_frame(&mut self, _frame_buffer: Vec<u8>) {
        // 不做任何事
    }
    fn render(&mut self) -> Result<(), String> {
        Ok(())
    }
    fn resize(&mut self, _width: u32, _height: u32) -> Result<(), String> {
        Ok(())
    }
    fn as_any_mut(&mut self) -> &mut dyn Any {
        self
    }
}
// DummyPixels 及 unsafe block 已移除，headless 模式僅用 DummyVideoInterface。

use std::any::Any;
use std::fmt::Debug;

pub trait VideoInterface: Debug + Any {
    fn update_frame(&mut self, frame_buffer: Vec<u8>);
    fn render(&mut self) -> Result<(), String>;
    fn resize(&mut self, new_width: u32, new_height: u32) -> Result<(), String>;
    fn as_any_mut(&mut self) -> &mut dyn Any;
}
