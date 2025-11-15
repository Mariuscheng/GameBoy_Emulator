#include "apu.h"
#include <iostream>
#include <cmath>

// ============================================================================
// APU Register Descriptor Table
// ============================================================================
// Defines read/write behavior for each APU register (0xFF10-0xFF26)
// Index maps directly: reg_table[0] = 0xFF10, reg_table[0x16] = 0xFF26

const APU::RegisterDescriptor APU::reg_table[0x17] = {
    // Channel 1 (Pulse with sweep)
    {0x7F, 0x7F, false, 0xFF},  // 0x00: NR10 (FF10) - sweep (bit 7 always 1)
    {0xC0, 0xFF, false, 0xFF},  // 0x01: NR11 (FF11) - duty+length (writes ignored when APU off)
    {0xFF, 0xFF, false, 0xFF},  // 0x02: NR12 (FF12) - envelope
    {0x00, 0xFF, false, 0xFF},  // 0x03: NR13 (FF13) - freq low (write-only)
    {0x40, 0xFF, false, 0xFF},  // 0x04: NR14 (FF14) - freq high+control (only bit 6 readable)
    
    // Unused
    {0x00, 0x00, false, 0xFF},  // 0x05: FF15 (unused)
    
    // Channel 2 (Pulse without sweep)
    {0xC0, 0xFF, false, 0xFF},  // 0x06: NR21 (FF16) - duty+length (writes ignored when APU off)
    {0xFF, 0xFF, false, 0xFF},  // 0x07: NR22 (FF17) - envelope
    {0x00, 0xFF, false, 0xFF},  // 0x08: NR23 (FF18) - freq low (write-only)
    {0x40, 0xFF, false, 0xFF},  // 0x09: NR24 (FF19) - freq high+control
    
    // Channel 3 (Wave)
    {0x80, 0x80, false, 0xFF},  // 0x0A: NR30 (FF1A) - DAC enable (only bit 7)
    {0x00, 0xFF, false, 0xFF},  // 0x0B: NR31 (FF1B) - length (writes ignored when APU off)
    {0x60, 0x60, false, 0xFF},  // 0x0C: NR32 (FF1C) - volume (bits 5-6 only)
    {0x00, 0xFF, false, 0xFF},  // 0x0D: NR33 (FF1D) - freq low (write-only)
    {0x40, 0xFF, false, 0xFF},  // 0x0E: NR34 (FF1E) - freq high+control
    
    // Unused
    {0x00, 0x00, false, 0xFF},  // 0x0F: FF1F (unused)
    
    // Channel 4 (Noise)
    {0x00, 0x3F, false, 0xFF},  // 0x10: NR41 (FF20) - length (writes ignored when APU off)
    {0xFF, 0xFF, false, 0xFF},  // 0x11: NR42 (FF21) - envelope
    {0xFF, 0xFF, false, 0xFF},  // 0x12: NR43 (FF22) - polynomial
    {0x40, 0xFF, false, 0xFF},  // 0x13: NR44 (FF23) - control
    
    // Sound control
    {0xFF, 0xFF, false, 0xFF},  // 0x14: NR50 (FF24) - master volume
    {0xFF, 0xFF, false, 0xFF},  // 0x15: NR51 (FF25) - panning
    {0x8F, 0x80, true,  0x70},  // 0x16: NR52 (FF26) - power+status (bit 7 writable, returns 0x70 when off)
};

// ============================================================================
// Constructor / Destructor
// ============================================================================

APU::APU() : frame_counter(0), frame_step(0), sample_timer(0.0), cycles_per_sample(CPU_CLOCK / SAMPLE_RATE) {
    reset();
}

APU::~APU() {
}

// ============================================================================
// Reset - Initialize to DMG power-on state
// ============================================================================

void APU::reset() {
    // Clear all registers (APU powered off at startup)
    for (int i = 0; i < 0x17; i++) {
        regs[i] = 0x00;
    }
    
    // DMG power-on defaults for master control
    regs[0x14] = 0x77;  // NR50: Left volume 7, Right volume 7
    regs[0x15] = 0xF3;  // NR51: Default panning
    regs[0x16] = 0x00;  // NR52: APU powered off
    
    // Initialize wave RAM to alternating pattern (DMG behavior)
    for (int i = 0; i < 16; i++) {
        wave_ram[i] = (i % 2 == 0) ? 0x00 : 0xFF;
    }
    
    // Initialize channel states
    ch1_state = {};
    ch2_state = {};
    ch3_state = {};
    ch4_state = {};
    
    // Reset legacy structures (for Phase 5 audio)
    ch1 = {};
    ch2 = {};
    ch3 = {};
    ch4 = {};
    
    // Initialize LFSR for noise channel
    ch4.lfsr = 0x7FFF;
    
    // Reset sample timing and buffers
    sample_timer = 0.0;
    audio_fifo.clear();
    apu_was_off = true;
}

void APU::step(int cycles) {
    frame_counter += cycles;

    // Frame sequencer runs at 512Hz (8192 cycles per step)
    while (frame_counter >= FRAME_SEQUENCER_PERIOD) {
        frame_counter -= FRAME_SEQUENCER_PERIOD;
        update_frame_sequencer();
    }

    // Update channel timers
    if (ch1.enabled) {
        ch1.timer += cycles;
        uint16_t period = (2048 - ch1.frequency) * 4;
        if (period == 0) period = 4;
        while (ch1.timer >= period) {
            ch1.timer -= period;
            ch1.position = (ch1.position + 1) % 8;
        }
    }

    if (ch2.enabled) {
        ch2.timer += cycles;
        uint16_t period = (2048 - ch2.frequency) * 4;
        if (period == 0) period = 4;
        while (ch2.timer >= period) {
            ch2.timer -= period;
            ch2.position = (ch2.position + 1) % 8;
        }
    }

    if (ch3.enabled) {
        ch3.timer += cycles;
        uint16_t period = (2048 - ch3.frequency) * 2;
        if (period == 0) period = 2;
        while (ch3.timer >= period) {
            ch3.timer -= period;
            ch3.position = (ch3.position + 1) % 32;
            uint8_t ram_index = (ch3.position / 2) & 0x0F;
            uint8_t sample_byte = ch3.wave_ram[ram_index];
            if (ch3.position % 2 == 0) {
                ch3.sample_buffer = sample_byte >> 4;
            } else {
                ch3.sample_buffer = sample_byte & 0x0F;
            }
        }
    }

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

    // Generate audio samples
    sample_timer += cycles;
    while (sample_timer >= cycles_per_sample) {
        sample_timer -= cycles_per_sample;
        mix_and_push_sample();
    }
}

// ============================================================================
// Register Read - Table-driven, power-state aware
// ============================================================================

uint8_t APU::read_register(uint16_t address) const {
    // === Wave RAM (0xFF30-0xFF3F) ===
    if (address >= 0xFF30 && address <= 0xFF3F) {
        // Wave RAM readable even when APU off
        // When CH3 enabled (on DMG), returns 0xFF
        if (apu_powered() && ch3_state.enabled) {
            return debug_read(address, 0xFF);
        }
        return debug_read(address, wave_ram[address - 0xFF30]);
    }
    
    // === APU Registers (0xFF10-0xFF26) ===
    if (address < 0xFF10 || address > 0xFF26) {
        return debug_read(address, 0xFF);  // Out of range
    }
    
    const uint8_t index = address - 0xFF10;
    const RegisterDescriptor& desc = reg_table[index];
    
    // APU off: most registers remain readable with normal masks; NR52 is special
    if (!apu_powered()) {
        if (address == 0xFF26) {
            // NR52: returns 0x70 | channel_status when off
            return debug_read(address, (uint8_t)(0x70 | get_channel_status()));
        }
        // Read back masked value from cleared storage
        uint8_t v = regs[index];
        v = (v & desc.read_mask) | (uint8_t)(~desc.read_mask);
        return debug_read(address, v);
    }

    // APU on: read from register storage
    uint8_t value = regs[index];

    // NR52 special handling: merge power bit with channel status
    if (address == 0xFF26) {
        value = (regs[0x16] & 0x80) | get_channel_status();
    }

    // Apply read mask: readable bits return actual value, others return 1
    value = (value & desc.read_mask) | (uint8_t)(~desc.read_mask);

    return debug_read(address, value);
}

// ============================================================================
// Helper Functions
// ============================================================================

uint8_t APU::get_channel_status() const {
    return (ch1_state.enabled ? 0x01 : 0) |
           (ch2_state.enabled ? 0x02 : 0) |
           (ch3_state.enabled ? 0x04 : 0) |
           (ch4_state.enabled ? 0x08 : 0);
}

void APU::power_off_apu() {
    // Clear NR52
    regs[0x16] = 0x00;
    
    // Clear all other APU registers (FF10-FF25)
    for (int i = 0; i < 0x16; i++) {
        regs[i] = 0x00;
    }
    
    // Disable all channels
    ch1_state.enabled = false;
    ch2_state.enabled = false;
    ch3_state.enabled = false;
    ch4_state.enabled = false;
    
    // Reset internal timers (optional, depends on implementation)
    ch1_state.timer = 0;
    ch2_state.timer = 0;
    ch3_state.timer = 0;
    ch4_state.timer = 0;
}

void APU::power_on_apu() {
    // Set NR52 power bit
    regs[0x16] = 0x80;
    
    // DMG behavior: channels remain disabled until triggered
    // Do NOT automatically enable channels here
    
    // Reset frame sequencer (next step will be step 0)
    frame_step = 7;
    frame_counter = FRAME_SEQUENCER_PERIOD - 1;
}

void APU::update_dac_state(int channel_num) {
    switch (channel_num) {
        case 1:
            ch1_state.dac_on = (regs[0x02] & 0xF0) != 0;  // NR12
            if (!ch1_state.dac_on) ch1_state.enabled = false;
            break;
        case 2:
            ch2_state.dac_on = (regs[0x07] & 0xF0) != 0;  // NR22
            if (!ch2_state.dac_on) ch2_state.enabled = false;
            break;
        case 3:
            ch3_state.dac_on = (regs[0x0A] & 0x80) != 0;  // NR30
            if (!ch3_state.dac_on) ch3_state.enabled = false;
            break;
        case 4:
            ch4_state.dac_on = (regs[0x11] & 0xF0) != 0;  // NR42
            if (!ch4_state.dac_on) ch4_state.enabled = false;
            break;
    }
}

void APU::trigger_channel(int channel_num) {
    // Simple trigger for Phase 1: just enable if DAC is on
    // Full trigger logic (length, envelope, etc.) in Phase 2
    ChannelState* state = nullptr;
    switch (channel_num) {
        case 1: state = &ch1_state; break;
        case 2: state = &ch2_state; break;
        case 3: state = &ch3_state; break;
        case 4: state = &ch4_state; break;
        default: return;
    }
    
    if (state->dac_on) {
        state->enabled = true;
    }
}

// ============================================================================
// Register Write - Table-driven, power-state aware
// ============================================================================

void APU::write_register(uint16_t address, uint8_t value) {
    // Wave RAM: always writable
    if (address >= 0xFF30 && address <= 0xFF3F) {
        int offset = address - 0xFF30;
        wave_ram[offset] = value;
        debug_log("WR", address, value);
        return;
    }
    
    // Map to flat register index
    if (address < 0xFF10 || address > 0xFF26) return;
    int reg_idx = address - 0xFF10;
    const auto& desc = reg_table[reg_idx];
    
    // Check APU power state
    bool apu_on = apu_powered();
    
    // If APU off and register not writable when off, ignore write
    if (!apu_on && !desc.writable_when_off) {
        debug_log("WR", address, value);
        return;
    }
    
    // Special case: NR52 power control
    if (address == 0xFF26) {
        bool power_on = (value & 0x80) != 0;
        
        if (power_on && !apu_on) {
            // Power on APU
            power_on_apu();
        } else if (!power_on && apu_on) {
            // Power off APU
            power_off_apu();
        }
        
        // Only write power bit, preserve channel status bits
        regs[reg_idx] = (value & 0x80) | get_channel_status();
        debug_log("WR", address, value);
        return;
    }
    
    // Apply write mask and update register
    regs[reg_idx] = (regs[reg_idx] & ~desc.write_mask) | (value & desc.write_mask);
    
    // Handle side effects based on register
    switch (address) {
        // DAC control registers
        case 0xFF12:  // NR12
            update_dac_state(1);
            break;
        case 0xFF17:  // NR22
            update_dac_state(2);
            break;
        case 0xFF1A:  // NR30
            update_dac_state(3);
            break;
        case 0xFF21:  // NR42
            update_dac_state(4);
            break;
            
        // Trigger registers
        case 0xFF14:  // NR14
            if (value & 0x80) trigger_channel(1);
            break;
        case 0xFF19:  // NR24
            if (value & 0x80) trigger_channel(2);
            break;
        case 0xFF1E:  // NR34
            if (value & 0x80) trigger_channel(3);
            break;
        case 0xFF23:  // NR44
            if (value & 0x80) trigger_channel(4);
            break;
    }
    
    debug_log("WR", address, value);
}

// ============================================================================
// Audio Generation (Phase 5 - TODO)
// ============================================================================

void APU::get_audio_samples(float* buffer, int length) {
    // Phase 1: Stub - just return silence
    for (int i = 0; i < length; i++) {
        buffer[i] = 0.0f;
    }
}

void APU::update_frame_sequencer() {
    // Phase 2: TODO - Length counter, sweep, envelope
}

void APU::update_sweep(PulseChannel& ch) {
    // Phase 2: TODO
}

void APU::update_envelope(PulseChannel& ch) {
    // Phase 2: TODO
}

void APU::update_envelope(NoiseChannel& ch) {
    // Phase 2: TODO
}

void APU::update_length(PulseChannel& ch) {
    // Phase 2: TODO
}

void APU::update_length(WaveChannel& ch) {
    // Phase 2: TODO
}

void APU::update_length(NoiseChannel& ch) {
    // Phase 2: TODO
}

float APU::generate_pulse_sample(const PulseChannel& ch) const {
    // Phase 5: TODO
    return 0.0f;
}

float APU::generate_wave_sample(const WaveChannel& ch) const {
    // Phase 5: TODO
    return 0.0f;
}

float APU::generate_noise_sample(const NoiseChannel& ch) const {
    // Phase 5: TODO
    return 0.0f;
}

uint8_t APU::get_duty_waveform(uint8_t duty, uint8_t position) const {
    // Phase 5: TODO
    return 0;
}

void APU::mix_and_push_sample() {
    // Phase 5: Stub - push silence
    audio_fifo.push_back(0.0f);
    audio_fifo.push_back(0.0f);
}
