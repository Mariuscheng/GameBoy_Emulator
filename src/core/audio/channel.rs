use crate::core::error::Result;

pub trait Channel {
    fn step(&mut self) -> Result<()>;
    fn get_sample(&self) -> f32;
    fn toggle(&mut self, enabled: bool);
}
