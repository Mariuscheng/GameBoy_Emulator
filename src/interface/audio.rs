use sdl3::audio::{AudioCallback, AudioFormat, AudioSpec, AudioStream};
use std::sync::{Arc, Mutex};

// === Minimal emulator-driven APU synth ===
// This is a very small, non-cycle-accurate mixer that generates a square tone for CH1,
// enough to get audible game sounds. It reads register state updated by the Bus.

#[derive(Debug, Clone)]
pub struct SimpleAPUSynth {
    pub master_enable: bool,   // NR52 bit7
    pub ch1_enable: bool,      // NR14 bit7 trigger (latched)
    pub ch1_freq_hz: f32,      // computed from NR13+NR14 (2048-n)
    pub ch1_duty: u8,          // 0..=3 -> 12.5/25/50/75%
    pub ch1_volume: f32,       // 0.0..1.0 (from NR12 initial volume)
    // runtime
    phase: f32,
}

impl Default for SimpleAPUSynth {
    fn default() -> Self {
        Self {
            master_enable: false,
            ch1_enable: false,
            ch1_freq_hz: 0.0,
            ch1_duty: 2, // 50%
            ch1_volume: 0.0,
            phase: 0.0,
        }
    }
}

impl SimpleAPUSynth {
    #[inline]
    fn duty_ratio(&self) -> f32 {
        match self.ch1_duty & 0x03 {
            0 => 0.125,
            1 => 0.25,
            2 => 0.50,
            _ => 0.75,
        }
    }
}

pub struct EmuAudioCallback {
    sample_rate: f32,
    synth: Arc<Mutex<SimpleAPUSynth>>,
}

impl AudioCallback<f32> for EmuAudioCallback {
    fn callback(&mut self, stream: &mut AudioStream, requested: i32) {
        let mut out = Vec::<f32>::with_capacity(requested as usize);
        let mut guard = self.synth.lock().unwrap();
        let s = &mut *guard;
        let sr = self.sample_rate;
        let duty = s.duty_ratio();
        for _ in 0..requested {
            let mut sample = 0.0f32;
            if s.master_enable && s.ch1_enable && s.ch1_freq_hz > 0.0 && s.ch1_volume > 0.0 {
                s.phase += s.ch1_freq_hz / sr;
                if s.phase >= 1.0 {
                    s.phase -= 1.0;
                }
                let v = if s.phase < duty { 1.0 } else { -1.0 };
                // modest global gain to avoid clipping
                sample += v * (s.ch1_volume * 0.25);
            }
            out.push(sample);
        }
        let _ = stream.put_data_f32(&out);
    }
}

#[allow(dead_code)]
pub struct AudioInterface {
    device: sdl3::audio::AudioStreamWithCallback<EmuAudioCallback>,
}

#[allow(dead_code)]
impl AudioInterface {
    /// Create audio output driven by the emulator synth state
    pub fn new_with_synth(synth: Arc<Mutex<SimpleAPUSynth>>) -> Result<Self, String> {
        let sdl_context = sdl3::init().map_err(|e| format!("SDL init error: {:?}", e))?;
        let audio_subsystem = sdl_context
            .audio()
            .map_err(|e| format!("SDL audio error: {:?}", e))?;

        let source_freq = 44100; // standard output sample rate
        let source_spec = AudioSpec {
            freq: Some(source_freq),
            channels: Some(1),                    // mono
            format: Some(AudioFormat::f32_sys()), // floating 32 bit samples
        };

        // Initialize the audio callback
        let device = audio_subsystem
            .open_playback_stream(
                &source_spec,
                EmuAudioCallback {
                    sample_rate: source_freq as f32,
                    synth,
                },
            )
            .map_err(|e| format!("Audio stream error: {:?}", e))?;

        Ok(Self { device })
    }

    pub fn start(&self) -> Result<(), String> {
        self.device
            .resume()
            .map_err(|e| format!("SDL resume error: {:?}", e))
    }

    pub fn stop(&self) -> Result<(), String> {
        self.device
            .pause()
            .map_err(|e| format!("SDL pause error: {:?}", e))
    }

    // Backward-compat helper kept no-op (test tone removed)
    pub fn play_test_tone(&self) {}
}

// Re-export synth for other modules (optional)


