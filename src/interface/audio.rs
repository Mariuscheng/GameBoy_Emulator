use sdl3::audio::{AudioCallback, AudioFormat, AudioSpec, AudioStream};
use std::time::Duration;

pub struct SquareWave {
    phase_inc: f32,
    phase: f32,
    volume: f32
}

impl AudioCallback<f32> for SquareWave {
    fn callback(&mut self, stream: &mut AudioStream, requested: i32) {
        let mut out = Vec::<f32>::with_capacity(requested as usize);
        // Generate a square wave
        for _ in 0..requested {
            out.push(if self.phase <= 0.5 {
                self.volume
            } else {
                -self.volume
            });
            self.phase = (self.phase + self.phase_inc) % 1.0;
        }
        stream.put_data_f32(&out);
    }
}

pub struct AudioInterface {
    device: sdl3::audio::AudioStreamWithCallback<SquareWave>,
}

impl AudioInterface {
    pub fn new() -> Result<Self, String> {
        let sdl_context = sdl3::init().map_err(|e| format!("SDL init error: {:?}", e))?;
        let audio_subsystem = sdl_context.audio().map_err(|e| format!("SDL audio error: {:?}", e))?;

        let source_freq = 44100;
        let source_spec = AudioSpec {
            freq: Some(source_freq),
            channels: Some(1),                      // mono
            format: Some(AudioFormat::f32_sys())    // floating 32 bit samples
        };

        // Initialize the audio callback
        let device = audio_subsystem.open_playback_stream(&source_spec, SquareWave {
            phase_inc: 440.0 / source_freq as f32,
            phase: 0.0,
            volume: 0.25
        }).map_err(|e| format!("Audio stream error: {:?}", e))?;

        Ok(Self { device })
    }

    pub fn start(&self) -> Result<(), String> {
        self.device.resume().map_err(|e| format!("SDL resume error: {:?}", e))
    }

    pub fn play_test_tone(&self) {
        // Play for 2 seconds
        std::thread::sleep(Duration::from_millis(2000));
    }
}