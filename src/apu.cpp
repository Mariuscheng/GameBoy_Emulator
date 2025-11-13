#include "apu.h"
#include <iostream>
#include <cmath>

APU::APU() : frame_counter(0), frame_step(0), sample_timer(0.0), cycles_per_sample(CPU_CLOCK / SAMPLE_RATE) {
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
    
    // NR52: Power control - APU disabled at power-on (matches real hardware)
    nr52 = 0x00;  // Bit 7 = 0 (APU disabled)

    // Initialize wave RAM to silence
    ch3.wave_ram.fill(0);

    // Initialize LFSR for noise channel
    ch4.lfsr = 0x7FFF;

    // Reset sample timing and buffers
    sample_timer = 0.0;
    audio_fifo.clear();
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
        if (period == 0) period = 4; // prevent infinite loop
        while (ch1.timer >= period) {
            ch1.timer -= period;
            ch1.position = (ch1.position + 1) % 8;
        }
    }

    // Channel 2
    if (ch2.enabled) {
        ch2.timer += cycles;
        uint16_t period = (2048 - ch2.frequency) * 4;
        if (period == 0) period = 4; // prevent infinite loop
        while (ch2.timer >= period) {
            ch2.timer -= period;
            ch2.position = (ch2.position + 1) % 8;
        }
    }

    // Channel 3
    if (ch3.enabled) {
        ch3.timer += cycles;
        uint16_t period = (2048 - ch3.frequency) * 2;
        if (period == 0) period = 2; // prevent infinite loop
        while (ch3.timer >= period) {
            ch3.timer -= period;
            ch3.position = (ch3.position + 1) % 32;
            // 防呆：確保 ram_index 不會越界
            uint8_t ram_index = (ch3.position / 2) & 0x0F; // 0~15
            uint8_t sample_byte = ch3.wave_ram[ram_index];
            if (ch3.position % 2 == 0) {
                ch3.sample_buffer = sample_byte >> 4;
            } else {
                ch3.sample_buffer = sample_byte & 0x0F;
            }
        }
    }

    // Channel 4 (Noise)
    if (ch4.enabled) {
        ch4.timer += cycles;
        // Hardware: dividing ratio codes: 0=>8, 1=>16, 2=>32, 3=>48, 4=>64, 5=>80, 6=>96, 7=>112
        uint8_t div_code = ch4.polynomial & 0x07;
        static const uint16_t div_table[8] = {8,16,32,48,64,80,96,112};
        uint16_t dividing_ratio = div_table[div_code];
        uint8_t shift = (ch4.polynomial >> 4) & 0x0F; // 0-15 but GB uses 0-13
        // Period in CPU cycles between LFSR advances:
        // frequency = 524288 / (dividing_ratio * 2^(shift+1)) -> period cycles = dividing_ratio * 2^(shift+1)
        uint16_t period = dividing_ratio << (shift + 1);
        while (ch4.timer >= period) {
            ch4.timer -= period;
            // Update LFSR (15-bit)
            uint8_t bit = (ch4.lfsr & 0x01) ^ ((ch4.lfsr >> 1) & 0x01);
            ch4.lfsr = (ch4.lfsr >> 1) | (bit << 14);
            if (ch4.polynomial & 0x08) { // Width mode: use 7-bit LFSR (tap into bit6)
                ch4.lfsr = (ch4.lfsr & ~(1 << 6)) | (bit << 6);
            }
        }
    }

    // Generate audio samples based on elapsed CPU cycles
    sample_timer += cycles;
    while (sample_timer >= cycles_per_sample) {
        sample_timer -= cycles_per_sample;
        mix_and_push_sample();
    }
}

uint8_t APU::read_register(uint16_t address) const {
    // When APU is powered off, all registers except wave RAM read as 0
    if (!(nr52 & 0x80) && !(address >= 0xFF30 && address <= 0xFF3F)) {
        debug_read(address, 0);
        return 0;
    }
    
    uint8_t val = 0xFF;
    switch (address) {
        case 0xFF10: val = ch1.sweep; break;
        case 0xFF11: val = ch1.length; break;
        case 0xFF12: val = ch1.envelope; break;
        case 0xFF13: val = 0xFF; break; // write-only low freq (return FF)
        case 0xFF14: val = ch1.frequency_hi; break;
        case 0xFF15: val = 0xFF; break;
        case 0xFF16: val = ch2.length; break;
        case 0xFF17: val = ch2.envelope; break;
        case 0xFF18: val = 0xFF; break; // write-only low freq
        case 0xFF19: val = ch2.frequency_hi; break;
        case 0xFF1A: val = ch3.dac_enabled; break;
        case 0xFF1B: val = ch3.length; break;
        case 0xFF1C: val = ch3.volume; break;
        case 0xFF1D: val = 0xFF; break; // write-only low freq
        case 0xFF1E: val = ch3.frequency_hi; break;
        case 0xFF1F: val = 0xFF; break;
        case 0xFF20: val = ch4.length; break;
        case 0xFF21: val = ch4.envelope; break;
        case 0xFF22: val = ch4.polynomial; break;
        case 0xFF23: val = ch4.control; break;
        case 0xFF24: val = nr50; break;
        case 0xFF25: val = nr51; break;
        case 0xFF26: {
            uint8_t status = 0;
            if (ch1.enabled) status |= 0x01;
            if (ch2.enabled) status |= 0x02;
            if (ch3.enabled) status |= 0x04;
            if (ch4.enabled) status |= 0x08;
            // DMG: bits 4-6 unused and read as 0.
            val = (nr52 & 0x80) | status; break;
        }
        default:
            if (address >= 0xFF30 && address <= 0xFF3F) {
                val = ch3.wave_ram[address - 0xFF30];
            }
            break;
    }
    
    // Apply read masks for DMG compatibility
    static const uint8_t read_masks[0x43] = {
        0x80, 0x3F, 0x00, 0xFF, 0xBF, 0xFF, 0x3F, 0x00, 0xFF, 0xBF, 0x7F, 0xFF, 0x9F, 0xFF, 0xBF, 0xFF, // NR10-NR1F
        0xFF, 0xFF, 0x3F, 0x00, 0xFF, 0xBF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, // NR20-NR2F
        0x7F, 0xFF, 0x9F, 0xFF, 0xBF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, // NR30-NR3F
        0xFF, 0xFF, 0x00, 0x00, 0xBF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, // NR40-NR4F
        0x00, 0x00, 0x00 // NR50-NR52
    };
    
    if (address >= 0xFF10 && address <= 0xFF52) {
        val |= read_masks[address - 0xFF10];
    } else if (address >= 0xFF30 && address <= 0xFF3F) {
        val |= 0x00; // Wave RAM mask
    }
    
    debug_read(address, val);
    return val;
}

void APU::write_register(uint16_t address, uint8_t value) {
    // When APU is powered off, writes to NR10-NR51 are ignored (except NR52 and wave RAM)
    if (!(nr52 & 0x80) && address != 0xFF26 && !(address >= 0xFF30 && address <= 0xFF3F)) {
        debug_log("WR", address, value);
        return;
    }

    switch (address) {
        case 0xFF10:
            ch1.sweep = value;
            debug_log("WR", address, value);
            // Update sweep enabled state (active only if shift>0 or period>0)
            ch1.sweep_enabled = ((value & 0x07) != 0) || (((value & 0x70) >> 4) != 0);
#if GB_APU_DEBUG
            std::cout << "[APU] CH1 sweep write value=0x" << std::hex << (int)value << std::dec
                      << " shift=" << (int)(value & 0x07)
                      << " period=" << (int)((value & 0x70) >> 4)
                      << " negate=" << ((value & 0x08)?1:0)
                      << std::endl;
#endif
            break;
        case 0xFF11:
            ch1.length = value;
            ch1.duty_cycle = value >> 6;
            debug_log("WR", address, value);
            break;
        case 0xFF12:
            ch1.envelope = value;
            debug_log("WR", address, value);
            // DAC enabled if any of bits 7-4 (initial volume) are non-zero (正確硬體規則)
            if ((value & 0xF0) == 0) {
                ch1.enabled = false; // DAC off immediately disables channel
#if GB_APU_DEBUG
                std::cout << "[APU] CH1 DAC disabled (NR12=0x" << std::hex << (int)value << std::dec << ")" << std::endl;
#endif
            }
            break;
        case 0xFF13:
            ch1.frequency_lo = value;
            debug_log("WR", address, value);
            break;
        case 0xFF14:
            ch1.frequency_hi = value;
            debug_log("WR", address, value);
            if (value & 0x80) { // Trigger
                if ((ch1.length_counter == 0) && (ch1.frequency_hi & 0x40)) {
                    ch1.length_counter = 64;
                } else {
                    ch1.length_counter = 64 - (ch1.length & 0x3F);
                }
                ch1.envelope_volume = (ch1.envelope >> 4) & 0x0F;
                ch1.envelope_counter = (ch1.envelope & 0x07) ? (ch1.envelope & 0x07) : 8;
                // 只要 DAC enable (envelope高4bit非0) 就設 enabled
                if ((ch1.envelope & 0xF0) != 0) {
                    ch1.enabled = true;
                } else {
                    ch1.enabled = false;
                }
                ch1.timer = 0;
                ch1.position = 0;
                ch1.sweep_frequency = ch1.frequency = ((ch1.frequency_hi & 0x07) << 8 | ch1.frequency_lo) & 0x7FF;
                ch1.sweep_counter = (ch1.sweep & 0x70) ? ((ch1.sweep & 0x70) >> 4) : 8;
            }
            break;
        case 0xFF16:
            ch2.length = value;
            ch2.duty_cycle = value >> 6;
            debug_log("WR", address, value);
            break;
        case 0xFF17:
            ch2.envelope = value;
            debug_log("WR", address, value);
            // DAC enabled if any of bits 7-4 (initial volume) are non-zero
            if ((value & 0xF0) == 0) {
                ch2.enabled = false; // DAC off immediately disables channel
#if GB_APU_DEBUG
                std::cout << "[APU] CH2 DAC disabled (NR22=0x" << std::hex << (int)value << std::dec << ")" << std::endl;
#endif
            }
            break;
        case 0xFF18:
            ch2.frequency_lo = value;
            debug_log("WR", address, value);
            break;
        case 0xFF19:
            ch2.frequency_hi = value;
            debug_log("WR", address, value);
            if (value & 0x80) { // Trigger
                ch2.frequency = ((ch2.frequency_hi & 0x07) << 8 | ch2.frequency_lo) & 0x7FF;
                if ((ch2.length_counter == 0) && (ch2.frequency_hi & 0x40)) {
                    ch2.length_counter = 64;
                } else {
                    ch2.length_counter = 64 - (ch2.length & 0x3F);
                }
                ch2.envelope_volume = static_cast<uint8_t>(ch2.envelope >> 4);
                ch2.envelope_counter = (ch2.envelope & 0x07) ? (ch2.envelope & 0x07) : 8;
                // 只要 DAC enable (envelope高4bit非0) 就設 enabled
                if ((ch2.envelope & 0xF8) != 0) {
                    ch2.enabled = true;
                } else {
                    ch2.enabled = false;
                }
                ch2.timer = 0;
                ch2.position = 0;
            }
            break;
        case 0xFF1A: 
            ch3.dac_enabled = (value & 0x80) ? 0x80 : 0x00;
            // If DAC is disabled, the channel is disabled immediately
            if (!ch3.dac_enabled) {
                ch3.enabled = false;
#if GB_APU_DEBUG
                std::cout << "[APU] CH3 DAC disabled (NR30=0x" << std::hex << (int)value << std::dec << ")" << std::endl;
#endif
            }
            debug_log("WR", address, value);
            break;
        case 0xFF1B:
            ch3.length = value;
            debug_log("WR", address, value);
            break;
        case 0xFF1C:
            ch3.volume = value;
            ch3.volume_shift = (value >> 5) & 0x03;
            debug_log("WR", address, value);
            break;
        case 0xFF1D:
            ch3.frequency_lo = value;
            debug_log("WR", address, value);
            break;
        case 0xFF1E:
            ch3.frequency_hi = value;
            debug_log("WR", address, value);
            if (value & 0x80) { // Trigger
                ch3.frequency = ((ch3.frequency_hi & 0x07) << 8 | ch3.frequency_lo) & 0x7FF;
                if ((ch3.length_counter == 0) && (ch3.frequency_hi & 0x40)) {
                    ch3.length_counter = static_cast<uint8_t>(256);
                } else {
                    ch3.length_counter = static_cast<uint8_t>(256 - ch3.length);
                }
                if (ch3.length_counter > static_cast<uint8_t>(256)) ch3.length_counter = static_cast<uint8_t>(256);
                ch3.timer = 0;
                ch3.position = 0; // 防呆：trigger時歸零
                ch3.sample_buffer = 0; // Clear buffer immediately
                // 只要 DAC enable 就設 enabled
                if (ch3.dac_enabled) {
                    ch3.enabled = true;
                } else {
                    ch3.enabled = false;
                }
            }
            break;
        case 0xFF20:
            ch4.length = value;
            debug_log("WR", address, value);
            break;
        case 0xFF21:
            ch4.envelope = value;
            debug_log("WR", address, value);
            // DAC enabled if any of bits 7-4 (initial volume) are non-zero
            if ((value & 0xF0) == 0) {
                ch4.enabled = false; // DAC off immediately disables channel
#if GB_APU_DEBUG
                std::cout << "[APU] CH4 DAC disabled (NR42=0x" << std::hex << (int)value << std::dec << ")" << std::endl;
#endif
            }
            break;
        case 0xFF22:
            ch4.polynomial = value;
            debug_log("WR", address, value);
            break;
        case 0xFF23:
            ch4.control = value;
            debug_log("WR", address, value);
            if (value & 0x80) { // Trigger
                ch4.length_counter = 64 - (ch4.length & 0x3F);
                ch4.envelope_volume = static_cast<uint8_t>(ch4.envelope >> 4);
                ch4.envelope_counter = (ch4.envelope & 0x07) ? (ch4.envelope & 0x07) : 8;
                // 只要 DAC enable (envelope高4bit非0) 就設 enabled
                if ((ch4.envelope & 0xF8) != 0) {
                    ch4.enabled = true;
                } else {
                    ch4.enabled = false;
                }
                ch4.timer = 0;
                ch4.lfsr = 0x7FFF;
                if ((ch4.length_counter == 0) && (ch4.control & 0x40)) {
                    ch4.length_counter = 64;
                }
                if (ch4.envelope_counter == 0) ch4.envelope_counter = 8;
                if (ch4.length_counter > 64) ch4.length_counter = 64;
            }
            break;
        case 0xFF24:
            nr50 = value;
            debug_log("WR", address, value);
            break;
        case 0xFF25:
            nr51 = value;
            debug_log("WR", address, value);
            break;
        case 0xFF26:
            // Bit7 controls APU power. Other bits are read-only status.
            debug_log("WR", address, value);
            if (!(value & 0x80)) {
                // Power off: disable channels & clear status bits but DO NOT zero user-writable registers (except status)
                nr52 = 0x00;
                ch1.enabled = ch2.enabled = ch3.enabled = ch4.enabled = false;
                // Shadow & timers cleared to avoid stale computations after power on
                // Note: On DMG, length counters are NOT cleared on power off
                ch1.sweep_frequency = 0; ch1.sweep_counter = 0; ch1.timer = 0; ch1.position = 0; ch1.envelope_counter = 0;
                ch2.timer = 0; ch2.position = 0; ch2.envelope_counter = 0;
                ch3.timer = 0; ch3.position = 0; ch3.sample_buffer = 0;
                ch4.timer = 0; ch4.envelope_counter = 0; ch4.lfsr = 0x7FFF;
                audio_fifo.clear();
                apu_was_off = true;
#if GB_APU_DEBUG
                std::cout << "[APU] POWER OFF" << std::endl;
#endif
            } else {
                if (apu_was_off) {
                    // Hardware leaves most registers unchanged; we keep previously written values but must reset internal counters.
                    ch1.timer = ch1.position = 0; ch1.sweep_counter = 0; ch1.envelope_counter = 0; ch1.sweep_frequency = ch1.frequency;
                    ch2.timer = ch2.position = 0; ch2.envelope_counter = 0;
                    ch3.timer = ch3.position = 0; ch3.sample_buffer = 0;
                    ch4.timer = 0; ch4.envelope_counter = 0; ch4.lfsr = 0x7FFF;
                    ch1.enabled = false; ch2.enabled = true; ch3.enabled = false; ch4.enabled = false; // channels start disabled until triggers
                    apu_was_off = false;
#if GB_APU_DEBUG
                    std::cout << "[APU] POWER ON (cold)" << std::endl;
#endif
                }
                nr52 = 0x80; // Power bit on, channel bits remain 0 until triggers
            }
            break;
        default:
            if (address >= 0xFF30 && address <= 0xFF3F) {
                ch3.wave_ram[address - 0xFF30] = value;
                debug_log("WR", address, value);
            }
            break;
    }
    // --- 強化：每次暫存器寫入後同步 NR52 channel enable bits，並防呆 ---
    nr52 = 0x80;
    if (ch1.enabled) nr52 |= 0x01;
    if (ch2.enabled) nr52 |= 0x02;
    if (ch3.enabled) nr52 |= 0x04;
    if (ch4.enabled) nr52 |= 0x08;
}

void APU::get_audio_samples(float* buffer, int length) {
    // Provide up to 'length' float samples (interleaved stereo) from FIFO.
    // If insufficient, output silence for remainder.
    for (int i = 0; i < length; i += 2) {
        if (audio_fifo.size() >= 2) {
            buffer[i] = audio_fifo.front(); audio_fifo.pop_front();
            buffer[i+1] = audio_fifo.front(); audio_fifo.pop_front();
        } else {
            buffer[i] = 0.0f; buffer[i+1] = 0.0f;
        }
    }
}

void APU::update_frame_sequencer() {
    frame_step = (frame_step + 1) % 8;
    // Length counters tick only on steps 0,2,4,6
    if (frame_step == 0 || frame_step == 2 || frame_step == 4 || frame_step == 6) {
        update_length(ch1);
        update_length(ch2);
        update_length(ch3);
        update_length(ch4);
    }

    // Sweep (every 2nd and 6th step)
    if (frame_step == 2 || frame_step == 6) {
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
    if (!ch.sweep_enabled) return;
    uint8_t raw_period = (ch.sweep & 0x70) >> 4; // bits 6-4
    uint8_t period = raw_period ? raw_period : 8; // HW treats 0 as 8 for timing
    uint8_t shift = ch.sweep & 0x07;

    if (ch.sweep_counter > 0) ch.sweep_counter--;
    if (ch.sweep_counter == 0) {
        ch.sweep_counter = period; // reload
        if (shift) {
            // First calculation
            uint16_t delta = ch.sweep_frequency >> shift;
            uint16_t new_freq = (ch.sweep & 0x08) ? (ch.sweep_frequency - delta) : (ch.sweep_frequency + delta);
            new_freq &= 0x7FF; // clamp to 11 bits
            if (new_freq > 2047) {
                ch.enabled = false; // immediate overflow
                return;
            }
            ch.sweep_frequency = new_freq;
            ch.frequency = new_freq;
            // Second calculation only for overflow test (without applying)
            uint16_t delta2 = ch.sweep_frequency >> shift;
            uint16_t test_freq = (ch.sweep & 0x08) ? (ch.sweep_frequency - delta2) : (ch.sweep_frequency + delta2);
            test_freq &= 0x7FF; // clamp to 11 bits
            if (test_freq > 2047) {
                ch.enabled = false; // will overflow next time -> disable now
            }
        }
    }
}

void APU::update_envelope(PulseChannel& ch) {
    uint8_t period = ch.envelope & 0x07;
    if (period == 0) return; // Period 0 => no automatic envelope changes
    if (ch.envelope_counter > 0) ch.envelope_counter--;
    if (ch.envelope_counter == 0) {
        ch.envelope_counter = period; // reload
        if (ch.envelope & 0x08) { // Increase
            if (ch.envelope_volume < 15) ch.envelope_volume++;
        } else { // Decrease
            if (ch.envelope_volume > 0) ch.envelope_volume--;
            if (ch.envelope_volume == 0) ch.enabled = false; // DAC disabled when volume reaches 0
        }
    }
}

void APU::update_envelope(NoiseChannel& ch) {
    uint8_t period = ch.envelope & 0x07;
    if (period == 0) return;
    if (ch.envelope_counter > 0) ch.envelope_counter--;
    if (ch.envelope_counter == 0) {
        ch.envelope_counter = period;
        if (ch.envelope & 0x08) {
            if (ch.envelope_volume < 15) ch.envelope_volume++;
        } else {
            if (ch.envelope_volume > 0) ch.envelope_volume--;
            if (ch.envelope_volume == 0) ch.enabled = false; // DAC disabled when volume reaches 0
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
            ch.enabled = false;
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

// Mix current channel state into one stereo sample and push it to the FIFO
void APU::mix_and_push_sample() {
    if (!(nr52 & 0x80)) { // APU powered off
        audio_fifo.push_back(0.0f); audio_fifo.push_back(0.0f);
        return;
    }

    float sample_left = 0.0f;
    float sample_right = 0.0f;

    // Mix channels based on panning (NR51)
    if (ch1.enabled) {
        float s = generate_pulse_sample(ch1);
        if (nr51 & 0x10) sample_left += s;
        if (nr51 & 0x01) sample_right += s;
    }
    if (ch2.enabled) {
        float s = generate_pulse_sample(ch2);
        if (nr51 & 0x20) sample_left += s;
        if (nr51 & 0x02) sample_right += s;
    }
    if (ch3.enabled && ch3.dac_enabled) {
        float s = generate_wave_sample(ch3);
        if (nr51 & 0x40) sample_left += s;
        if (nr51 & 0x04) sample_right += s;
    }
    if (ch4.enabled) {
        float s = generate_noise_sample(ch4);
        if (nr51 & 0x80) sample_left += s;
        if (nr51 & 0x08) sample_right += s;
    }

    // Master volume (NR50)
    float left_vol = ((nr50 >> 4) & 0x07) / 7.0f;
    float right_vol = (nr50 & 0x07) / 7.0f;

    // Clamp
    if (sample_left > 1.0f) sample_left = 1.0f; if (sample_left < -1.0f) sample_left = -1.0f;
    if (sample_right > 1.0f) sample_right = 1.0f; if (sample_right < -1.0f) sample_right = -1.0f;

    // Simple 1st-order high-pass filter to remove DC offset (z^{-1} form)
    static float hp_prev_l = 0.0f, hp_prev_r = 0.0f;
    static float hp_prev_out_l = 0.0f, hp_prev_out_r = 0.0f;
    const float RC = 0.999f; // close to 1 => very low cutoff
    float in_l = sample_left * left_vol;
    float in_r = sample_right * right_vol;
    float out_l = RC * (hp_prev_out_l + in_l - hp_prev_l);
    float out_r = RC * (hp_prev_out_r + in_r - hp_prev_r);
    hp_prev_l = in_l; hp_prev_r = in_r; hp_prev_out_l = out_l; hp_prev_out_r = out_r;

    audio_fifo.push_back(out_l * AMPLITUDE);
    audio_fifo.push_back(out_r * AMPLITUDE);
}
