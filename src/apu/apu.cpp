#include "apu.h"
#include <iostream>
#include <cmath>


const APU::RegisterDescriptor APU::reg_table[0x17] = {
    {0x7F, 0x7F, false, 0x80},  // 0x00: NR10 (FF10) - sweep (bits 0-6 readable, bit 7 defaults to 1)
    {0xFF, 0xFF, true, 0xBF},  // 0x01: NR11 (FF11) - duty+length (writes ignored when APU off, always read 0xBF initially)
    {0xFF, 0xFF, false, 0xF3},  // 0x02: NR12 (FF12) - envelope
    {0x00, 0xFF, false, 0x00},  // 0x03: NR13 (FF13) - freq low (write-only)
    {0xC7, 0xC7, false, 0xBF},  // 0x04: NR14 (FF14) - freq high+control (bit 6 readable, others 1)
    
    {0x00, 0x00, false, 0x00},  // 0x05: FF15 (unused)
    
    {0xFF, 0xFF, true, 0x3F},  // 0x06: NR21 (FF16) - duty+length
    {0xFF, 0xFF, false, 0x00},  // 0x07: NR22 (FF17) - envelope
    {0x00, 0xFF, false, 0x00},  // 0x08: NR23 (FF18) - freq low (write-only)
    {0xC7, 0xC7, false, 0xBF},  // 0x09: NR24 (FF19) - freq high+control
    
    {0x80, 0x80, false, 0x7F},  // 0x0A: NR30 (FF1A) - DAC enable (only bit 7)
    {0xFF, 0xFF, true, 0xFF},  // 0x0B: NR31 (FF1B) - length (full read)
    {0x60, 0x60, false, 0x9F},  // 0x0C: NR32 (FF1C) - volume (bits 5-6 only)
    {0x00, 0xFF, false, 0x00},  // 0x0D: NR33 (FF1D) - freq low (write-only)
    {0xC7, 0xC7, false, 0xBF},  // 0x0E: NR34 (FF1E) - freq high+control
    
    {0x00, 0x00, false, 0x00},  // 0x0F: FF1F (unused)
    
    {0xFF, 0xFF, true, 0x3F},  // 0x10: NR41 (FF20) - length
    {0xFF, 0xFF, false, 0x00},  // 0x11: NR42 (FF21) - envelope
    {0xFF, 0xFF, false, 0x00},  // 0x12: NR43 (FF22) - polynomial
    {0xC7, 0xC7, false, 0xBF},  // 0x13: NR44 (FF23) - control
    
    {0xFF, 0xFF, false, 0x77},  // 0x14: NR50 (FF24) - master volume
    {0xFF, 0xFF, false, 0xF3},  // 0x15: NR51 (FF25) - panning
    {0x8F, 0x80, true,  0xF0},  // 0x16: NR52 (FF26) - power+status (bit 7 writable, all bits readable)
};


APU::APU() : frame_counter(0), frame_step(0), sample_timer(0.0), cycles_per_sample(CPU_CLOCK / SAMPLE_RATE) {
    reset();
}

APU::~APU() {
}


void APU::reset() {
    std::memset(regs.data(), 0, regs.size());
    
    regs[0x14] = 0x77;  // NR50
    regs[0x15] = 0xF3;  // NR51
    regs[0x16] = 0x80;  // NR52: APU on (bit7=1, status=0)
    
    for (int i = 0; i < 16; i++) {
        wave_ram[i] = (i % 2 == 0) ? 0x00 : 0xFF;
    }
    
    ch1_state = {};
    ch2_state = {};
    ch3_state = {};
    ch4_state = {};
    
    ch1 = {};
    ch2 = {};
    ch3 = {};
    ch4 = {};
    
    ch4.lfsr = 0x7FFF;
    
    sample_timer = 0.0;
    audio_fifo.clear();
    frame_step = 7;  // 開機 step 7
    frame_counter = 0;
    apu_was_off = false;
}

void APU::step(int cycles) {
    sample_timer += cycles;
    while (sample_timer >= cycles_per_sample) {
        sample_timer -= cycles_per_sample;
        mix_and_push_sample();
    }

    frame_counter += cycles;

    while (frame_counter >= FRAME_SEQUENCER_PERIOD) {
        frame_counter -= FRAME_SEQUENCER_PERIOD;
        update_frame_sequencer();
    }

    update_pulse_timer(ch1, cycles);
    update_pulse_timer(ch2, cycles);
    update_wave_timer(ch3, cycles);

    if (ch4.enabled) {
        ch4.timer += cycles;
        uint8_t div_code = ch4.polynomial & 0x07;
        static const uint16_t div_table[8] = {8,16,32,48,64,80,96,112};
        uint16_t dividing_ratio = div_table[div_code];
        uint8_t shift = (ch4.polynomial >> 4) & 0x0F;
        uint16_t period = dividing_ratio << (shift + 1);
        while (ch4.timer >= period) {
            ch4.timer -= period;
            uint8_t bit = (ch4.lfsr & 0x01) ^ ((ch4.lfsr >> 1) & 0x01);
            ch4.lfsr = (ch4.lfsr >> 1) | (bit << 14);
            if (ch4.polynomial & 0x08) {
                ch4.lfsr = (ch4.lfsr & ~(1 << 6)) | (bit << 6);
            }
        }
    }

    sample_timer += cycles;
    while (sample_timer >= cycles_per_sample) {
        sample_timer -= cycles_per_sample;
        mix_and_push_sample();
    }
}


void APU::update_pulse_timer(PulseChannel& ch, int cycles) {
    if (!ch.enabled) return;
    ch.timer += cycles;
    uint16_t period = (2048 - ch.frequency) * 4;
    if (period == 0) period = 4;
    while (ch.timer >= period) {
        ch.timer -= period;
        ch.position = (ch.position + 1) % 8;
    }
}

void APU::update_wave_timer(WaveChannel& ch, int cycles) {
    if (!ch.enabled) return;
    ch.timer += cycles;
    uint16_t period = (2048 - ch.frequency) * 2;
    if (period == 0) period = 2;
    while (ch.timer >= period) {
        ch.timer -= period;
        ch.position = (ch.position + 1) % 32;
        uint8_t ram_index = (ch.position / 2) & 0x0F;
        uint8_t sample_byte = wave_ram[ram_index];
        if (ch.position % 2 == 0) {
            ch.sample_buffer = sample_byte >> 4;
        } else {
            ch.sample_buffer = sample_byte & 0x0F;
        }
    }
}


uint8_t APU::read_register(uint16_t address) const {
    if (address >= 0xFF30 && address <= 0xFF3F) {
        return debug_read(address, read_wave_ram(address));
    }
    
    if (address < 0xFF10 || address > 0xFF26) {
        return debug_read(address, 0xFF);  // Out of range
    }
    
    const uint8_t index = address - 0xFF10;
    const RegisterDescriptor& desc = reg_table[index];
    
    if (!apu_powered()) {
        if (address == 0xFF26) {
            const_cast<APU*>(this)->flush_for_nr52_read();
            return debug_read(address, (uint8_t)(0x70 | get_channel_status()));
        }
        return debug_read(address, desc.default_read);
    }

    uint8_t value = regs[index];

    if (address == 0xFF26) {
        const_cast<APU*>(this)->flush_for_nr52_read();
        value = (regs[0x16] & 0x80) | get_channel_status();
    }

    value = (value & desc.read_mask) | (desc.default_read & ~desc.read_mask);

    return debug_read(address, value);
}



uint8_t APU::get_channel_status() const {
    uint8_t status = 0;
    bool ch1_len_en = (regs[0x04] & 0x40) != 0;
    bool ch2_len_en = (regs[0x09] & 0x40) != 0;
    bool ch3_len_en = (regs[0x0E] & 0x40) != 0;
    bool ch4_len_en = (regs[0x13] & 0x40) != 0;
    if (ch1_state.enabled && (!ch1_len_en || ch1.length_counter != 0) && ch1_state.dac_on) status |= 0x01;
    if (ch2_state.enabled && (!ch2_len_en || ch2.length_counter != 0) && ch2_state.dac_on) status |= 0x02;
    if (ch3_state.enabled && (!ch3_len_en || ch3.length_counter != 0) && ch3_state.dac_on) status |= 0x04;
    if (ch4_state.enabled && (!ch4_len_en || ch4.length_counter != 0) && ch4_state.dac_on) status |= 0x08;
    return status;
}

void APU::flush_for_nr52_read() {
    if ((regs[0x04] & 0x40) && ch1.length_counter == 0) { ch1_state.enabled = false; ch1.enabled = 0; }
    if ((regs[0x09] & 0x40) && ch2.length_counter == 0) { ch2_state.enabled = false; ch2.enabled = 0; }
    if ((regs[0x0E] & 0x40) && ch3.length_counter == 0) { ch3_state.enabled = false; ch3.enabled = 0; }
    if ((regs[0x13] & 0x40) && ch4.length_counter == 0) { ch4_state.enabled = false; ch4.enabled = 0; }
}

void APU::power_off_apu() {
    regs[0x16] &= ~0x80;

    ch1_state.enabled = false;
    ch2_state.enabled = false;
    ch3_state.enabled = false;
    ch4_state.enabled = false;

    ch1.enabled = 0;
    ch2.enabled = 0;
    ch3.enabled = 0;
    ch4.enabled = 0;
}

void APU::power_on_apu() {
    regs[0x16] |= 0x80;
    
    
    frame_step = 7;
    frame_counter = 0;
}

void APU::update_dac_state(int channel_num) {
    switch (channel_num) {
        case 1:
            ch1_state.dac_on = (regs[0x02] & 0xF0) != 0;  // NR12
            break;
        case 2:
            ch2_state.dac_on = (regs[0x07] & 0xF0) != 0;  // NR22
            break;
        case 3:
            ch3_state.dac_on = (regs[0x0A] & 0x80) != 0;  // NR30
            break;
        case 4:
            ch4_state.dac_on = (regs[0x11] & 0xF0) != 0;  // NR42
            break;
    }
}

void APU::update_dac_state(uint16_t address, uint8_t value) {
    switch (address) {
        case NR11:  // NR11 ch1.length = value; ch1.duty = (value >> 6) & 0x03;
            ch1.length_counter = 64 - (value & 0x3F);
            break;
        case NR12:  // NR12
            ch1.envelope = value;
            ch1.envelope_volume = (value >> 4) & 0x0F;
            ch1.envelope_increase = (value & 0x08) != 0;
            ch1.envelope_period = value & 0x07;
            break;
        case NR22: ch2_state.dac_on = (regs[0x07] & 0xF8) != 0; break;
        case NR30: ch3_state.dac_on = (regs[0x0A] & 0x80) != 0; break;
        case NR42: ch4_state.dac_on = (regs[0x11] & 0xF8) != 0; break;
    }
}


uint8_t APU::read_wave_ram(uint16_t address) const {
    if (ch3_state.enabled) {
        return 0xFF;
    }
    return wave_ram[address - 0xFF30];
}

void APU::write_wave_ram(uint16_t address, uint8_t value) {
    if (apu_powered() && ch3_state.enabled) {
        debug_log("WR", address, value);  // Ignore writes when CH3 is enabled
        return;
    }
    wave_ram[address - 0xFF30] = value;
    debug_log("WR", address, value);
}

void APU::trigger_channel(int channel_num) {
    // GameBoy APU Channel Trigger Logic
    // When a channel's NRx4 register is written with bit 7 set (trigger bit),
    // the channel is restarted according to GameBoy specifications.
    // This implements the trigger behavior for all four APU channels.

    initialize_channel_state(channel_num);
    reload_length_counter(channel_num);
    initialize_sweep(channel_num);
    initialize_envelope(channel_num);
    reset_timers_and_phase(channel_num);
    check_immediate_sweep_overflow(channel_num);
}

void APU::initialize_channel_state(int channel_num) {
    ChannelState* state = nullptr;
    PulseChannel* pch = nullptr;
    WaveChannel*  wch = nullptr;
    NoiseChannel* nch = nullptr;
    switch (channel_num) {
        case 1: state = &ch1_state; pch = &ch1; break;
        case 2: state = &ch2_state; pch = &ch2; break;
        case 3: state = &ch3_state; wch = &ch3; break;
        case 4: state = &ch4_state; nch = &ch4; break;
        default: return;
    }
    
    // Enable the channel in the APU state
    state->enabled = true;
    if (pch) pch->enabled = 1;
    if (wch) wch->enabled = 1;
    if (nch) nch->enabled = 1;
}

void APU::reload_length_counter(int channel_num) {
    // Reload length counter if it was zero
    // GameBoy spec: If length counter is 0 when triggered, reload from NRx1 register
    switch (channel_num) {
        case 1:
            if (ch1.length_counter == 0) {
                uint8_t n = regs[0x01] & 0x3F; // NR11 low 6 bits (length data)
                ch1.length_counter = (n == 0) ? 64 : static_cast<uint8_t>(64 - n);
            }
            break;
        case 2:
            if (ch2.length_counter == 0) {
                uint8_t n = regs[0x06] & 0x3F; // NR21 low 6 bits (length data)
                ch2.length_counter = (n == 0) ? 64 : static_cast<uint8_t>(64 - n);
            }
            break;
        case 3:
            if (ch3.length_counter == 0) {
                uint8_t n = regs[0x0B]; // NR31 full 8 bits (wave channel uses 8-bit length)
                ch3.length_counter = (n == 0) ? 256 : static_cast<uint16_t>(256 - n);
            }
            break;
        case 4:
            if (ch4.length_counter == 0) {
                uint8_t n = regs[0x10] & 0x3F; // NR41 low 6 bits (length data)
                ch4.length_counter = (n == 0) ? 64 : static_cast<uint8_t>(64 - n);
            }
            break;
    }
}

void APU::initialize_sweep(int channel_num) {
    // Initialize sweep for channel 1
    // GameBoy sweep: Period and shift from NR10, frequency from current NR13/NR14
    if (channel_num == 1) {
        uint8_t period = (regs[0x00] >> 4) & 7;  // NR10 bits 4-6: sweep period
        uint8_t shift = regs[0x00] & 7;          // NR10 bits 0-2: sweep shift
        ch1.sweep_counter = period ? period : 8; // 0 means 8
        ch1.sweep_frequency = ch1.frequency;     // Shadow register
        ch1.sweep_enabled = (period != 0 || shift != 0);
    }
}

void APU::initialize_envelope(int channel_num) {
    // Initialize envelope for applicable channels
    // Envelope: Volume from NRx2 bits 4-7, period from bits 0-2
    if (channel_num == 1) {
        ch1.envelope_volume = (regs[0x02] >> 4);  // NR12 initial volume
        ch1.envelope_counter = (regs[0x02] & 7);  // NR12 period
    } else if (channel_num == 2) {
        ch2.envelope_volume = (regs[0x07] >> 4);  // NR22 initial volume
        ch2.envelope_counter = (regs[0x07] & 7);  // NR22 period
    } else if (channel_num == 4) {
        ch4.envelope_volume = (regs[0x11] >> 4);  // NR42 initial volume
        ch4.envelope_counter = (regs[0x11] & 7);  // NR42 period
    }
}

void APU::reset_timers_and_phase(int channel_num) {
    // Reset phase/timers
    if (channel_num == 1 || channel_num == 2) {
        PulseChannel* pch = (channel_num == 1) ? &ch1 : &ch2;
        pch->position = 0;  // Reset duty cycle position
        pch->timer = 0;     // Reset frequency timer
    } else if (channel_num == 3) {
        ch3.position = 0;    // Reset wave position
        ch3.timer = 0;       // Reset frequency timer
        ch3.sample_buffer = 0;
    } else if (channel_num == 4) {
        ch4.timer = 0;       // Reset frequency timer
        ch4.lfsr = 0x7FFF;   // Reset LFSR to all 1s (GameBoy spec)
    }
}

void APU::check_immediate_sweep_overflow(int channel_num) {
    // Check for immediate sweep overflow (channel 1 only)
    // GameBoy bug/feature: If sweep shift > 0 and calculation overflows, disable immediately
    if (channel_num == 1 && ch1.sweep_shift != 0) {
        uint16_t calc_freq = ch1.sweep_frequency;
        if (ch1.sweep_direction) {
            calc_freq -= calc_freq >> ch1.sweep_shift;  // Subtract mode
        } else {
            calc_freq += calc_freq >> ch1.sweep_shift;  // Add mode
        }
        if (calc_freq > 2047) {  // Frequency overflow (>11 bits)
            ch1.sweep_enabled = false;
            ch1.enabled = false;
            ch1_state.enabled = false;
            regs[0x16] &= ~0x01;  // Clear channel 1 bit in NR52
        }
    }
}


void APU::handle_length_trigger(uint16_t address, uint8_t value, uint8_t old_reg, int channel_num) {
    uint8_t reg_offset;
    ChannelState* state;
    bool is_wave = false;

    switch (channel_num) {
        case 1: reg_offset = 0x04; state = &ch1_state; break;
        case 2: reg_offset = 0x09; state = &ch2_state; break;
        case 3: reg_offset = 0x0E; state = &ch3_state; is_wave = true; break;
        case 4: reg_offset = 0x13; state = &ch4_state; break;
        default: return;
    }

    bool prev_len_en = (old_reg & 0x40) != 0;
    bool new_len_en = (regs[reg_offset] & 0x40) != 0;
    bool next_is_len_tick = (((frame_step + 1) & 1) == 0);

    uint16_t old_len;
    if (is_wave) {
        old_len = ch3.length_counter;
    } else if (channel_num == 1) {
        old_len = ch1.length_counter;
    } else if (channel_num == 2) {
        old_len = ch2.length_counter;
    } else {
        old_len = ch4.length_counter;
    }

    if (!next_is_len_tick && !prev_len_en && new_len_en && old_len > 0) {
        if (is_wave) {
            ch3.length_counter--;
        } else if (channel_num == 1) {
            ch1.length_counter--;
        } else if (channel_num == 2) {
            ch2.length_counter--;
        } else {
            ch4.length_counter--;
        }
        uint16_t new_len = is_wave ? ch3.length_counter : (channel_num == 1 ? ch1.length_counter : (channel_num == 2 ? ch2.length_counter : ch4.length_counter));
        if (new_len == 0 && !(value & 0x80)) {
            state->enabled = false;
            if (channel_num == 1) ch1.enabled = 0;
            else if (channel_num == 2) ch2.enabled = 0;
            else if (channel_num == 3) ch3.enabled = 0;
            else if (channel_num == 4) ch4.enabled = 0;
        }
    }

    bool was_zero = (old_len == 0);
    if (value & 0x80) {
        trigger_channel(channel_num);
        if (!next_is_len_tick && !prev_len_en && new_len_en && was_zero) {
            // Extra length decrement logic if needed
        }
    }

    if (channel_num == 1 || channel_num == 2) {
        PulseChannel& ch = (channel_num == 1) ? ch1 : ch2;
        ch.length_enabled = new_len_en;
        if (state->enabled) {
            ch.frequency = ((regs[reg_offset] & 7) << 8) | regs[reg_offset - 1];
            if (channel_num == 1) ch1.sweep_frequency = ch1.frequency;
        }
    }
}


void APU::handle_sweep(uint8_t value) {
    ch1.sweep_period = (value >> 4) & 0x07;
    ch1.sweep_direction = (value & 0x08) != 0;
    ch1.sweep_shift = value & 0x07;
    ch1.sweep_counter = ch1.sweep_period ? ch1.sweep_period : 8;
}

void APU::handle_duty_length(int channel_num, uint8_t value) {
    if (channel_num == 1) {
        uint8_t n = regs[0x01] & 0x3F;
        uint8_t old_len = ch1.length_counter;
        ch1.length_counter = (n == 0) ? 64 : static_cast<uint8_t>(64 - n);
    } else if (channel_num == 2) {
        uint8_t n = regs[0x06] & 0x3F;
        uint8_t old_len = ch2.length_counter;
        ch2.length_counter = (n == 0) ? 64 : static_cast<uint8_t>(64 - n);
    }
}

void APU::handle_envelope(int channel_num, uint8_t value) {
    update_dac_state(channel_num);
}

void APU::handle_wave_length(uint8_t value) {
    uint8_t n = regs[0x0B];
    uint16_t old_len = ch3.length_counter;
    ch3.length_counter = (n == 0) ? 256 : static_cast<uint16_t>(256 - n);
}

void APU::handle_noise_length(uint8_t value) {
    uint8_t n = regs[0x10] & 0x3F;
    uint8_t old_len = ch4.length_counter;
    ch4.length_counter = (n == 0) ? 64 : static_cast<uint8_t>(64 - n);
}

void APU::handle_frequency_low(int channel_num, uint8_t value) {
    if (channel_num == 1 && ch1_state.enabled) {
        ch1.frequency = ((regs[0x04] & 7) << 8) | regs[0x03];
        ch1.sweep_frequency = ch1.frequency;
    } else if (channel_num == 2 && ch2_state.enabled) {
        ch2.frequency = ((regs[0x09] & 7) << 8) | regs[0x08];
    } else if (channel_num == 3 && ch3_state.enabled) {
        ch3.frequency = ((regs[0x0E] & 7) << 8) | regs[0x0D];
    }
    // Channel 4 (noise) doesn't use frequency low
}

void APU::handle_wave_on_off(uint8_t value) {
    update_dac_state(3);
}


void APU::write_register(uint16_t address, uint8_t value) {
    bool is_len_reg = (address == NR11 || address == NR21 || address == NR31 || address == NR41 || address == NR14 || address == NR24 || address == NR34 || address == NR44 || address == NR52);
    if (address >= 0xFF30 && address <= 0xFF3F) {
        write_wave_ram(address, value);
        return;
    }
    
    if (address < 0xFF10 || address > 0xFF26) {return;} // Out of range

    uint8_t reg_idx = address - 0xFF10;
    const RegisterDescriptor& desc = reg_table[reg_idx];
    
    bool apu_on = apu_powered();
    
    if (!apu_on && !desc.writable_when_off && address != 0xFF26) {
        debug_log("WR", address, value);
        return;  // 完全忽略
    }

    uint8_t old_reg = regs[reg_idx];  // 原始舊值（用於 side effect 判斷）

    if (!apu_on && (address == NR11 || address == NR21)) {
        regs[reg_idx] = (old_reg & 0xC0) | (value & 0x3F);// 保留原有 duty (bits 6-7)，只更新 length (bits 0-5)
    } else {
        regs[reg_idx] = (old_reg & ~desc.write_mask) | (value & desc.write_mask);
    } 
    
    if (address == 0xFF26) {
        bool power_on = (value & 0x80) != 0;
        if (power_on && !apu_on) {
            power_on_apu();
        } else if (!power_on && apu_on) {
            power_off_apu();
        }
        
        regs[reg_idx] = (value & 0x80) | get_channel_status();
        debug_log("WR", address, value);
        return;
    }
    
    regs[reg_idx] = (regs[reg_idx] & ~desc.write_mask) | (value & desc.write_mask);
    
    switch (address) {
        case NR10: handle_sweep(value); break;
        case NR11: handle_duty_length(1, value); break;
        case NR12: handle_envelope(1, value); break;
        case NR13: handle_frequency_low(1, value); break;
        case NR21: handle_duty_length(2, value); break;
        case NR22: handle_envelope(2, value); break;
        case NR23: handle_frequency_low(2, value); break;
        case NR30: handle_wave_on_off(value); break;
        case NR31: handle_wave_length(value); break;
        case NR33: handle_frequency_low(3, value); break;
        case NR41: handle_noise_length(value); break;
        case NR42: handle_envelope(4, value); break;
        case NR43: /* Noise polynomial, no special handling */ break;
        case NR14: handle_length_trigger(address, value, old_reg, 1); break;
        case NR24: handle_length_trigger(address, value, old_reg, 2); break;
        case NR34: handle_length_trigger(address, value, old_reg, 3); break;
        case NR44: handle_length_trigger(address, value, old_reg, 4); break;
    }

    handle_side_effects(address, value, old_reg);
    
    debug_log("WR", address, value);
}

void APU::handle_side_effects(uint16_t address, uint8_t value, uint8_t old_reg) {
    switch (address) {
        case 0xFF11:  // NR11
            handle_nr11_side_effects(value);
            break;
        case NR21:  // NR21
            handle_nr21_side_effects(value);
            break;
        case NR31:  // NR31
            handle_nr31_side_effects(value);
            break;
        case NR41:  // NR41
            handle_nr41_side_effects(value);
            break;
    }
    update_dac_state(address, value);  // 更新 dac_on
}

void APU::handle_nr11_side_effects(uint8_t value) {
    ch1.duty = (value >> 6) & 0x03;  // duty
    ch1.length_counter = 64 - (value & 0x3F);  // 【關鍵】reload length
}

void APU::handle_nr21_side_effects(uint8_t value) {
    ch2.duty = (value >> 6) & 0x03;
    ch2.length_counter = 64 - (value & 0x3F);
}

void APU::handle_nr31_side_effects(uint8_t value) {
    ch3.length_counter = 256 - value;
}

void APU::handle_nr41_side_effects(uint8_t value) {
    ch4.length_counter = 64 - (value & 0x3F);
}



void APU::get_audio_samples(float* buffer, int length) {
    bool powered = apu_powered();
    if (!powered) {
        for (int i = 0; i < length; i++) buffer[i] = 0.0f;
        return;
    }

    for (int i = 0; i < length; i++) {
        if (!audio_fifo.empty()) {
            buffer[i] = audio_fifo.front();
            audio_fifo.pop_front();
        } else {
            buffer[i] = 0.0f;
        }
    }
}

void APU::update_frame_sequencer() {
    frame_step = (frame_step + 1) & 7;

    if (GB_APU_DEBUG) {
        bool len_tick = ((frame_step & 1) == 0);
        if (!this->debug_len_only || len_tick) {
        }
    }

    if ((frame_step & 1) == 0) {
        update_length(1);
        update_length(2);
        update_length(3);
        update_length(4);
    }

    if (frame_step == 2 || frame_step == 6) {
        update_sweep(ch1);
    }
    if (frame_step == 7) {
        update_envelope(ch1);
        update_envelope(ch2);
        update_envelope(ch4);
    }
}

void APU::update_sweep(PulseChannel& ch) {
    // GameBoy APU Sweep Logic (Channel 1 only)
    // Sweep modifies the channel's frequency over time based on NR10 settings.
    // The sweep period determines how often the frequency is updated.
    // Direction: 0=add to frequency, 1=subtract from frequency
    // Shift: Number of bits to shift right for the frequency delta
    // Formula: new_freq = old_freq +/- (old_freq >> shift)
    
    if (!ch.sweep_enabled) return;

    ch.sweep_counter--;
    if (ch.sweep_counter == 0) {
        // Calculate new frequency based on sweep parameters
        uint16_t new_freq = ch.sweep_frequency;
        if (ch.sweep_direction) {
            new_freq -= new_freq >> ch.sweep_shift;  // Subtract mode
        } else {
            new_freq += new_freq >> ch.sweep_shift;  // Add mode
        }

        // GameBoy sweep bug: In subtract mode, if shift > 0 and the subtraction
        // would borrow from bit 15 (underflow), disable sweep immediately
        if (ch.sweep_direction && new_freq > ch.sweep_frequency) {
            ch.sweep_enabled = false;
            ch.enabled = false;
            if (&ch == &ch1) ch1_state.enabled = false;
            regs[NR52 - 0xFF10] &= ~(1 << 0);  // Clear CH1 bit in NR52
            return;
        }

        // Check for frequency overflow (>2047, exceeds 11-bit range)
        if (new_freq > 2047) {
            ch.sweep_enabled = false;
            ch.enabled = false;
            if (&ch == &ch1) ch1_state.enabled = false;
            regs[NR52 - 0xFF10] &= ~(1 << 0);  // Clear CH1 bit in NR52
        } else {
            // Apply the new frequency
            ch.frequency = new_freq;
            ch.sweep_frequency = new_freq;  // Update shadow register
            
            // Update the hardware registers NR13 and NR14 to reflect the change
            regs[NR13 - 0xFF10] = new_freq & 0xFF;  // Low 8 bits to NR13
            regs[NR14 - 0xFF10] = (regs[NR14 - 0xFF10] & 0xF8) | ((new_freq >> 8) & 7);  // High 3 bits to NR14
        }

        // Reset sweep counter for next update
        ch.sweep_counter = ch.sweep_period;
    }
}

void APU::update_envelope(PulseChannel& ch) {
    uint8_t period = ch.envelope_period;
    if (period == 0) return; // no envelope updates
    if (ch.envelope_counter == 0) {
        ch.envelope_counter = period;
        if (ch.envelope_increase) {
            if (ch.envelope_volume < 15) ch.envelope_volume++;
        } else {
            if (ch.envelope_volume > 0) ch.envelope_volume--;
        }
        if (GB_APU_DEBUG && !this->debug_len_only) {
        }
    } else {
        ch.envelope_counter--;
    }
}

void APU::update_envelope(NoiseChannel& ch) {
    uint8_t period = ch.envelope_period;
    if (period == 0) return;
    if (ch.envelope_counter == 0) {
        ch.envelope_counter = period;
        if (ch.envelope_increase) {
            if (ch.envelope_volume < 15) ch.envelope_volume++;
        } else {
            if (ch.envelope_volume > 0) ch.envelope_volume--;
        }
        if (GB_APU_DEBUG && !this->debug_len_only) {
        }
    } else {
        ch.envelope_counter--;
    }
}

void APU::update_length(int channel_num) {
    uint8_t reg_offset;
    ChannelState* state;
    bool is_pulse = false;
    bool is_wave = false;
    bool is_noise = false;

    switch (channel_num) {
        case 1: reg_offset = 0x04; state = &ch1_state; is_pulse = true; break;
        case 2: reg_offset = 0x09; state = &ch2_state; is_pulse = true; break;
        case 3: reg_offset = 0x0E; state = &ch3_state; is_wave = true; break;
        case 4: reg_offset = 0x13; state = &ch4_state; is_noise = true; break;
        default: return;
    }

    bool len_en = (regs[reg_offset] & 0x40) != 0;
    if (!len_en) return;

    uint16_t* length_counter;
    if (is_wave) {
        length_counter = &ch3.length_counter;
    } else if (channel_num == 1) {
        length_counter = reinterpret_cast<uint16_t*>(&ch1.length_counter);
    } else if (channel_num == 2) {
        length_counter = reinterpret_cast<uint16_t*>(&ch2.length_counter);
    } else {
        length_counter = reinterpret_cast<uint16_t*>(&ch4.length_counter);
    }

    if (*length_counter == 0) return;
    (*length_counter)--;
    if (*length_counter == 0) {
        state->enabled = false;
        if (channel_num == 1) ch1.enabled = 0;
        else if (channel_num == 2) ch2.enabled = 0;
        else if (channel_num == 3) ch3.enabled = 0;
        else if (channel_num == 4) ch4.enabled = 0;

        if (is_wave) {
            state->dac_on = (regs[0x0A] & 0x80) != 0;
        } else if (is_pulse) {
            uint8_t envelope_reg = (channel_num == 1) ? regs[0x02] : regs[0x07];
            state->dac_on = (envelope_reg & 0xF0) != 0;
        } else if (is_noise) {
            state->dac_on = (ch4.envelope & 0xF0) != 0;
        }

        if (GB_APU_DEBUG) {
        }
    }
}

float APU::generate_pulse_sample(const PulseChannel& ch) const {
    if (!ch.enabled) return 0.0f;
    uint8_t duty = (ch.length >> 6) & 3; // NRx1 duty
    uint8_t waveform = get_duty_waveform(duty, ch.position);
    float sample = waveform ? 1.0f : -1.0f;
    sample *= ch.envelope_volume / 15.0f;
    return sample * AMPLITUDE;
}

float APU::generate_wave_sample(const WaveChannel& ch) const {
    if (!ch.enabled || !ch3_state.dac_on) return 0.0f;
    uint8_t sample = ch.sample_buffer;
    float f_sample = (sample / 7.5f) - 1.0f;
    uint8_t nr32 = regs[0x0C]; // NR32
    uint8_t volume_code = (nr32 >> 5) & 3;
    static const float volumes[4] = {0.0f, 1.0f, 0.5f, 0.25f};
    f_sample *= volumes[volume_code];
    return f_sample * AMPLITUDE;
}

float APU::generate_noise_sample(const NoiseChannel& ch) const {
    if (!ch.enabled) return 0.0f;
    uint8_t bit = ch.lfsr & 1;
    float sample = bit ? 1.0f : -1.0f;
    sample *= ch.envelope_volume / 15.0f;
    return sample * AMPLITUDE;
}

uint8_t APU::get_duty_waveform(uint8_t duty, uint8_t position) const {
    static const uint8_t waveforms[4][8] = {
        {0, 0, 0, 0, 0, 0, 0, 1}, // 12.5%
        {0, 0, 0, 0, 0, 0, 1, 1}, // 25%
        {0, 0, 0, 0, 1, 1, 1, 1}, // 50%
        {1, 1, 1, 1, 1, 1, 0, 0}  // 75%
    };
    return waveforms[duty & 3][position & 7];
}

void APU::mix_and_push_sample() {
    float sample = 0.0f;
    if (apu_powered()) {
        sample += generate_pulse_sample(ch1);
        sample += generate_pulse_sample(ch2);
        sample += generate_wave_sample(ch3);
        sample += generate_noise_sample(ch4);
        uint8_t nr50 = regs[0x14]; // NR50
        uint8_t left_vol = nr50 & 7;
        uint8_t right_vol = (nr50 >> 4) & 7;
        sample *= (left_vol + right_vol) / 14.0f;
    }
    audio_fifo.push_back(sample);
}
