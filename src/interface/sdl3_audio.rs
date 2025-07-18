use crate::interface::audio::AudioInterface;
use sdl3::audio::{AudioCallback, AudioDevice};
use std::fmt;
use std::sync::{Arc, Mutex};

const AUDIO_BUFFER_SIZE: usize = 2048;

struct AudioCallbackHandler {
    buffer: Arc<Mutex<Vec<f32>>>,
}

impl AudioCallback for AudioCallbackHandler {
    fn callback(&mut self, out: &mut [f32]) {
        if let Ok(mut buffer) = self.buffer.lock() {
            let len = buffer.len().min(out.len());
            if len > 0 {
                out[..len].copy_from_slice(&buffer[..len]);
                buffer.drain(..len);
            }
            // 如果緩衝區數據不足，用靜音填充
            for sample in out[len..].iter_mut() {
                *sample = 0.0;
            }
        }
    }
}

pub struct Sdl3AudioOutput {
    audio_device: Option<AudioDevice<AudioCallbackHandler>>,
    buffer: Arc<Mutex<Vec<f32>>>,
}

impl Sdl3AudioOutput {
    pub fn new(sample_rate: u32) -> Self {
        let sdl_context = sdl3::init().unwrap();
        let audio_subsystem = sdl_context.audio().unwrap();

        let buffer = Arc::new(Mutex::new(Vec::with_capacity(AUDIO_BUFFER_SIZE)));
        let callback = AudioCallbackHandler {
            buffer: Arc::clone(&buffer),
        };

        let device = audio_subsystem
            .open_playback::<f32, _>(
                None,
                sample_rate as i32,
                1,    // mono
                4096, // buffer size
                callback,
            )
            .ok();

        if let Some(device) = &device {
            device.resume();
        }

        Self {
            audio_device: device,
            buffer,
        }
    }
}

impl AudioInterface for Sdl3AudioOutput {
    fn queue_samples(&mut self, samples: &[f32]) {
        if let Ok(mut buffer) = self.buffer.lock() {
            buffer.extend_from_slice(samples);
        }
    }

    fn resume(&mut self) {
        if let Some(device) = &self.audio_device {
            device.resume();
        }
    }

    fn pause(&mut self) {
        if let Some(device) = &self.audio_device {
            device.pause();
        }
    }
}

impl fmt::Debug for Sdl3AudioOutput {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.debug_struct("Sdl3AudioOutput")
            .field("buffer_size", &AUDIO_BUFFER_SIZE)
            .finish()
    }
}

impl fmt::Debug for Sdl3AudioOutput {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.debug_struct("Sdl3AudioOutput")
            .field("has_device", &self.audio_device.is_some())
            .finish()
    }
}

impl AudioInterface for Sdl3AudioOutput {
    fn push_sample(&mut self, sample: f32) {
        // 直接發送樣本到音頻回調
        if let Some(sender) = &self.sender {
            let _ = sender.send(sample);
        }
    }

    fn start(&mut self) {
        if let Some(device) = &self.audio_device {
            device.resume();
        }
    }

    fn stop(&mut self) {
        if let Some(device) = &self.audio_device {
            device.pause();
        }
    }
}

impl Drop for Sdl3AudioOutput {
    fn drop(&mut self) {
        self.stop();
    }
}
