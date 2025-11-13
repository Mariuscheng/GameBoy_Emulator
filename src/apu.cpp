#include "apu.h"
#include <iostream>
#include <cmath>

APU::APU() : frame_counter(0), frame_step(0) {
    reset();
}

APU::~APU() {
}

void APU::reset() {
    // Initialize all registers to 0
    ch1 = {};
    ch2 = {};
    ch3 = {};
    ch4 = {};
    
    // Initialize sound control registers to reasonable defaults
    // NR50: Master volume - set left and right volume to max (bits 4-6 and 0-2)
    nr50 = 0x77;  // Left volume: 7, Right volume: 7
    
    // NR51: Sound panning - enable all channels on both speakers
    nr51 = 0xFF;  // All channels on left and right
    
    // NR52: Power control - enable APU
    nr52 = 0x80;  // Bit 7 = 1 (APU enabled)

    // Initialize wave RAM to silence
    ch3.wave_ram.fill(0);

    // Initialize LFSR for noise channel
    ch4.lfsr = 0x7FFF;
}

void APU::step(int cycles) {
    frame_counter += cycles;

    // Frame sequencer runs at 512Hz (8192 cycles per step)
    while (frame_counter >= 8192) {
        frame_counter -= 8192;
        update_frame_sequencer();
    }

    // Update channel timers
    // Channel 1
    if (ch1.enabled) {
        ch1.timer += cycles;
        uint16_t period = (2048 - ch1.frequency) * 4;
        while (ch1.timer >= period) {
            ch1.timer -= period;
            ch1.position = (ch1.position + 1) % 8;
        }
    }

    // Channel 2
    if (ch2.enabled) {
        ch2.timer += cycles;
        uint16_t period = (2048 - ch2.frequency) * 4;
        while (ch2.timer >= period) {
            ch2.timer -= period;
            ch2.position = (ch2.position + 1) % 8;
        }
    }

    // Channel 3
    if (ch3.channel_enabled) {
        ch3.timer += cycles;
        uint16_t period = (2048 - ch3.frequency) * 2;
        while (ch3.timer >= period) {
            ch3.timer -= period;
            ch3.position = (ch3.position + 1) % 32;
            // Load next sample
            uint8_t ram_index = ch3.position / 2;
            uint8_t sample_byte = ch3.wave_ram[ram_index];
            if (ch3.position % 2 == 0) {
                ch3.sample_buffer = sample_byte >> 4;
            } else {
                ch3.sample_buffer = sample_byte & 0x0F;
            }
        }
    }

    // Channel 4
    if (ch4.enabled) {
        ch4.timer += cycles;
        uint16_t divisor = [this]() {
            uint8_t div_code = ch4.polynomial & 0x07;
            return div_code == 0 ? 8 : (16 << div_code);
        }();
        uint16_t period = divisor << ((ch4.polynomial >> 4) + 1);
        while (ch4.timer >= period) {
            ch4.timer -= period;
            // Update LFSR
            uint8_t bit = (ch4.lfsr & 0x01) ^ ((ch4.lfsr >> 1) & 0x01);
            ch4.lfsr = (ch4.lfsr >> 1) | (bit << 14);
            if (ch4.polynomial & 0x08) {
                ch4.lfsr = (ch4.lfsr & ~(1 << 6)) | (bit << 6);
            }
        }
    }
}

uint8_t APU::read_register(uint16_t address) const {
    switch (address) {
        case 0xFF10: return ch1.sweep;
        case 0xFF11: return ch1.length;
        case 0xFF12: return ch1.envelope;
        case 0xFF13: return ch1.frequency_lo;
        case 0xFF14: return ch1.frequency_hi;
        case 0xFF15: return 0xFF; // Unused
        case 0xFF16: return ch2.length;
        case 0xFF17: return ch2.envelope;
        case 0xFF18: return ch2.frequency_lo;
        case 0xFF19: return ch2.frequency_hi;
        case 0xFF1A: return ch3.dac_enabled;
        case 0xFF1B: return ch3.length;
        case 0xFF1C: return ch3.volume;
        case 0xFF1D: return ch3.frequency_lo;
        case 0xFF1E: return ch3.frequency_hi;
        case 0xFF1F: return 0xFF; // Unused
        case 0xFF20: return ch4.length;
        case 0xFF21: return ch4.envelope;
        case 0xFF22: return ch4.polynomial;
        case 0xFF23: return ch4.control;
        case 0xFF24: return nr50;
        case 0xFF25: return nr51;
        case 0xFF26: return nr52 | 0x70; // Bits 7, 4-6 always set
        case 0xFF27: case 0xFF28: case 0xFF29: case 0xFF2A: case 0xFF2B:
        case 0xFF2C: case 0xFF2D: case 0xFF2E: case 0xFF2F:
            return 0xFF; // Unused
        default:
            if (address >= 0xFF30 && address <= 0xFF3F) {
                return ch3.wave_ram[address - 0xFF30];
            }
            return 0xFF;
    }
}

void APU::write_register(uint16_t address, uint8_t value) {
    switch (address) {
        case 0xFF10: ch1.sweep = value; break;
        case 0xFF11:
            ch1.length = value;
            ch1.duty_cycle = value >> 6;
            break;
        case 0xFF12: ch1.envelope = value; break;
        case 0xFF13: ch1.frequency_lo = value; break;
        case 0xFF14:
            ch1.frequency_hi = value;
            if (value & 0x80) { // Trigger
                // Reset channel
                ch1.frequency = (ch1.frequency_hi & 0x07) << 8 | ch1.frequency_lo;
                ch1.length_counter = 64 - (ch1.length & 0x3F);
                ch1.envelope_volume = ch1.envelope >> 4;
                ch1.envelope_counter = ch1.envelope & 0x07;
                ch1.enabled = (ch1.envelope & 0xF8) != 0; // DAC enabled
                ch1.timer = 0;
                ch1.position = 0;
                // Sweep
                ch1.sweep_frequency = ch1.frequency;
                ch1.sweep_counter = (ch1.sweep & 0x70) >> 4;
                ch1.sweep_enabled = (ch1.sweep & 0x70) != 0;
            }
            break;
        case 0xFF16:
            ch2.length = value;
            ch2.duty_cycle = value >> 6;
            break;
        case 0xFF17: ch2.envelope = value; break;
        case 0xFF18: ch2.frequency_lo = value; break;
        case 0xFF19:
            ch2.frequency_hi = value;
            if (value & 0x80) { // Trigger
                ch2.frequency = (ch2.frequency_hi & 0x07) << 8 | ch2.frequency_lo;
                ch2.length_counter = 64 - (ch2.length & 0x3F);
                ch2.envelope_volume = ch2.envelope >> 4;
                ch2.envelope_counter = ch2.envelope & 0x07;
                ch2.enabled = (ch2.envelope & 0xF8) != 0;
                ch2.timer = 0;
                ch2.position = 0;
            }
            break;
        case 0xFF1A: 
            ch3.dac_enabled = value & 0x80; 
            // If DAC is disabled, the channel is disabled
            if (!ch3.dac_enabled) {
                ch3.channel_enabled = false;
            }
            break;
        case 0xFF1B: ch3.length = value; break;
        case 0xFF1C:
            ch3.volume = value;
            ch3.volume_shift = (value >> 5) & 0x03;
            break;
        case 0xFF1D: ch3.frequency_lo = value; break;
        case 0xFF1E:
            ch3.frequency_hi = value;
            if (value & 0x80) { // Trigger
                ch3.frequency = (ch3.frequency_hi & 0x07) << 8 | ch3.frequency_lo;
                ch3.length_counter = 256 - ch3.length;
                ch3.channel_enabled = (ch3.dac_enabled & 0x80) != 0;
                ch3.timer = 0;
                ch3.position = 0;
            }
            break;
        case 0xFF20: ch4.length = value; break;
        case 0xFF21: ch4.envelope = value; break;
        case 0xFF22: ch4.polynomial = value; break;
        case 0xFF23:
            ch4.control = value;
            if (value & 0x80) { // Trigger
                ch4.length_counter = 64 - (ch4.length & 0x3F);
                ch4.envelope_volume = ch4.envelope >> 4;
                ch4.envelope_counter = ch4.envelope & 0x07;
                ch4.enabled = (ch4.envelope & 0xF8) != 0;
                ch4.timer = 0;
                ch4.lfsr = 0x7FFF;
            }
            break;
        case 0xFF24: nr50 = value; break;
        case 0xFF25: nr51 = value; break;
        case 0xFF26:
            nr52 = value & 0x80; // Only bit 7 is writable
            if (!(value & 0x80)) {
                // Reset all channels
                ch1.enabled = ch2.enabled = ch3.channel_enabled = ch4.enabled = false;
            }
            break;
        default:
            if (address >= 0xFF30 && address <= 0xFF3F) {
                ch3.wave_ram[address - 0xFF30] = value;
            }
            break;
    }
}

void APU::get_audio_samples(float* buffer, int length) {
    // Generate audio samples based on current channel state
    for (int i = 0; i < length; i += 2) {
        float sample_left = 0.0f;
        float sample_right = 0.0f;

        // Mix channels based on panning (NR51)
        if (ch1.enabled) {
            float ch1_sample = generate_pulse_sample(ch1);
            if (nr51 & 0x01) sample_right += ch1_sample;      // Channel 1 on right
            if (nr51 & 0x10) sample_left += ch1_sample;       // Channel 1 on left
        }

        if (ch2.enabled) {
            float ch2_sample = generate_pulse_sample(ch2);
            if (nr51 & 0x02) sample_right += ch2_sample;      // Channel 2 on right
            if (nr51 & 0x20) sample_left += ch2_sample;       // Channel 2 on left
        }

        if (ch3.channel_enabled && (ch3.dac_enabled & 0x80)) {
            float ch3_sample = generate_wave_sample(ch3);
            if (nr51 & 0x04) sample_right += ch3_sample;      // Channel 3 on right
            if (nr51 & 0x40) sample_left += ch3_sample;       // Channel 3 on left
        }

        if (ch4.enabled) {
            float ch4_sample = generate_noise_sample(ch4);
            if (nr51 & 0x08) sample_right += ch4_sample;      // Channel 4 on right
            if (nr51 & 0x80) sample_left += ch4_sample;       // Channel 4 on left
        }

        // Apply master volume from NR50 (0xFF24)
        // Bits 6-4: Left volume (0-7)
        // Bits 2-0: Right volume (0-7)
        float left_vol = ((nr50 >> 4) & 0x07) / 7.0f;
        float right_vol = (nr50 & 0x07) / 7.0f;

        // Clamp to prevent overflow
        sample_left = (sample_left > 1.0f) ? 1.0f : sample_left;
        sample_right = (sample_right > 1.0f) ? 1.0f : sample_right;

        buffer[i] = sample_left * left_vol * AMPLITUDE;           // Left channel
        buffer[i + 1] = sample_right * right_vol * AMPLITUDE;     // Right channel
    }
}

void APU::update_frame_sequencer() {
    frame_step = (frame_step + 1) % 8;

    // Length counter (every step)
    update_length(ch1);
    update_length(ch2);
    update_length(ch3);
    update_length(ch4);

    // Sweep (every 2nd and 6th step)
    if (frame_step % 4 == 2) {
        update_sweep(ch1);
    }

    // Envelope (every 7th step)
    if (frame_step == 7) {
        update_envelope(ch1);
        update_envelope(ch2);
        update_envelope(ch4);
    }
}

void APU::update_sweep(PulseChannel& ch) {
    if (!ch.sweep_enabled || ch.sweep_counter == 0) return;

    ch.sweep_counter--;
    if (ch.sweep_counter == 0) {
        ch.sweep_counter = (ch.sweep & 0x70) >> 4;
        if (ch.sweep_counter == 0) ch.sweep_counter = 8;

        if (ch.sweep_enabled && (ch.sweep & 0x07)) {
            uint16_t delta = ch.sweep_frequency >> (ch.sweep & 0x07);
            if (ch.sweep & 0x08) { // Negative sweep
                ch.sweep_frequency -= delta;
            } else {
                ch.sweep_frequency += delta;
            }

            if (ch.sweep_frequency > 2047) {
                ch.enabled = false;
            } else {
                ch.frequency = ch.sweep_frequency;
            }
        }
    }
}

void APU::update_envelope(PulseChannel& ch) {
    if (ch.envelope_counter == 0) return;

    ch.envelope_counter--;
    if (ch.envelope_counter == 0) {
        ch.envelope_counter = ch.envelope & 0x07;
        if (ch.envelope_counter == 0) ch.envelope_counter = 8;

        if (ch.envelope & 0x08) { // Increase
            if (ch.envelope_volume < 15) ch.envelope_volume++;
        } else { // Decrease
            if (ch.envelope_volume > 0) ch.envelope_volume--;
        }
    }
}

void APU::update_envelope(NoiseChannel& ch) {
    if (ch.envelope_counter == 0) return;

    ch.envelope_counter--;
    if (ch.envelope_counter == 0) {
        ch.envelope_counter = ch.envelope & 0x07;
        if (ch.envelope_counter == 0) ch.envelope_counter = 8;

        if (ch.envelope & 0x08) { // Increase
            if (ch.envelope_volume < 15) ch.envelope_volume++;
        } else { // Decrease
            if (ch.envelope_volume > 0) ch.envelope_volume--;
        }
    }
}

void APU::update_length(PulseChannel& ch) {
    if ((ch.frequency_hi & 0x40) && ch.length_counter > 0) { // Length enabled
        ch.length_counter--;
        if (ch.length_counter == 0) {
            ch.enabled = false;
        }
    }
}

void APU::update_length(WaveChannel& ch) {
    if ((ch.frequency_hi & 0x40) && ch.length_counter > 0) { // Length enabled
        ch.length_counter--;
        if (ch.length_counter == 0) {
            ch.channel_enabled = false;
        }
    }
}

void APU::update_length(NoiseChannel& ch) {
    if ((ch.control & 0x40) && ch.length_counter > 0) { // Length enabled
        ch.length_counter--;
        if (ch.length_counter == 0) {
            ch.enabled = false;
        }
    }
}

float APU::generate_pulse_sample(const PulseChannel& ch) const {
    uint8_t wave = get_duty_waveform(ch.duty_cycle, ch.position);
    return wave ? (ch.envelope_volume / 15.0f) : 0.0f;
}

float APU::generate_wave_sample(const WaveChannel& ch) const {
    // Wave channel volume shift: 
    // 0 = mute (output 0)
    // 1 = 100% volume (no shift)
    // 2 = 50% volume (shift right by 1)
    // 3 = 25% volume (shift right by 2)
    
    if (ch.volume_shift == 0) {
        return 0.0f; // Muted
    }
    
    float sample = ch.sample_buffer / 15.0f; // 4-bit sample normalized to 0-1
    
    // Apply volume shift (volume_shift 1 means no shift, 2 means /2, 3 means /4)
    if (ch.volume_shift > 1) {
        sample = sample / (1 << (ch.volume_shift - 1));
    }
    
    return sample;
}

float APU::generate_noise_sample(const NoiseChannel& ch) const {
    uint8_t bit = ch.lfsr & 0x01;
    return bit ? 0.0f : (ch.envelope_volume / 15.0f);
}

uint8_t APU::get_duty_waveform(uint8_t duty, uint8_t position) const {
    static const uint8_t waveforms[4][8] = {
        {0, 0, 0, 0, 0, 0, 0, 1}, // 12.5%
        {1, 0, 0, 0, 0, 0, 0, 1}, // 25%
        {1, 0, 0, 0, 0, 1, 1, 1}, // 50%
        {0, 1, 1, 1, 1, 1, 1, 0}  // 75%
    };
    return waveforms[duty][position];
}