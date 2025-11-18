#ifndef APU_H
#define APU_H

#include <array>
#include <deque>
#include <cstdint>
#include <iostream>

// Debug macro for APU
#ifndef GB_APU_DEBUG
#define GB_APU_DEBUG 0
#endif

class APU {
public:
    APU();
    ~APU();

    void reset();
    void step(int cycles);
    uint8_t read_register(uint16_t address) const;
    void write_register(uint16_t address, uint8_t value);

    void get_audio_samples(float* buffer, int length);

private:
    // Constants
    static constexpr int CPU_CLOCK = 4194304;
    static constexpr int SAMPLE_RATE = 44100;
    static constexpr float AMPLITUDE = 0.1f;

    // Frame sequencer constants
    static constexpr int FRAME_SEQUENCER_PERIOD = 8192; // 512Hz

    // Channel structures
    struct PulseChannel {
        // Registers
        uint8_t sweep;         // NR10 (0xFF10) - sweep
        uint8_t length;        // NR11 (0xFF11) - duty + length
        uint8_t envelope;      // NR12 (0xFF12) - envelope
        uint8_t frequency_lo;  // NR13 (0xFF13) - freq low
        uint8_t frequency_hi;  // NR14 (0xFF14) - freq hi + control

        // Internal state
        uint16_t frequency;
        uint8_t duty_cycle;
        uint8_t length_counter;
        uint8_t envelope_counter;
        uint8_t envelope_volume;
        uint16_t sweep_counter;
        uint16_t sweep_frequency;
        uint8_t sweep_enabled;
        uint8_t enabled;
        uint16_t timer;
        uint8_t position;
    };

    struct WaveChannel {
        // Registers
        uint8_t dac_enabled;    // NR30 (0xFF1A) - DAC enable
        uint8_t length;         // NR31 (0xFF1B) - length
        uint8_t volume;         // NR32 (0xFF1C) - volume
        uint8_t frequency_lo;   // NR33 (0xFF1D) - freq low
        uint8_t frequency_hi;   // NR34 (0xFF1E) - freq hi + control

        // Wave RAM (32 samples, 4-bit each)
        std::array<uint8_t, 16> wave_ram; // 0xFF30-0xFF3F

        // Internal state
        uint16_t frequency;
        uint8_t length_counter;
        uint8_t volume_shift;
        uint8_t enabled;
        uint16_t timer;
        uint8_t position;
        uint8_t sample_buffer;
    };

    struct NoiseChannel {
        // Registers
        uint8_t length;         // NR41 (0xFF20) - length
        uint8_t envelope;       // NR42 (0xFF21) - envelope
        uint8_t polynomial;     // NR43 (0xFF22) - polynomial
        uint8_t control;        // NR44 (0xFF23) - control

        // Internal state
        uint8_t length_counter;
        uint8_t envelope_counter;
        uint8_t envelope_volume;
        uint8_t enabled;
        uint16_t timer;
        uint16_t lfsr;          // Linear Feedback Shift Register
    };

    // Channels
    PulseChannel ch1;
    PulseChannel ch2;
    WaveChannel ch3;
    NoiseChannel ch4;

    // Sound control
    uint8_t nr50; // Master volume & VIN panning (0xFF24)
    uint8_t nr51; // Sound panning (0xFF25)
    uint8_t nr52; // Sound on/off (0xFF26)

    // Frame sequencer
    int frame_counter;
    int frame_step;

    // Sample timing
    double sample_timer;
    double cycles_per_sample;

    // Audio buffer
    std::deque<float> audio_fifo;

    // Power state tracking
    bool apu_was_off;

    // Helper functions
    void update_frame_sequencer();
    void update_sweep(PulseChannel& ch);
    void update_envelope(PulseChannel& ch);
    void update_envelope(NoiseChannel& ch);
    void update_length(PulseChannel& ch);
    void update_length(WaveChannel& ch);
    void update_length(NoiseChannel& ch);

    // Unified channel trigger
    void trigger_channel(PulseChannel& ch, uint8_t length_reg, bool is_ch1 = false);
    void trigger_channel(WaveChannel& ch);
    void trigger_channel(NoiseChannel& ch);

    // DAC check
    bool dac_enabled(const PulseChannel& ch) const { return (ch.envelope & 0xF0) != 0; }
    bool dac_enabled(const WaveChannel& ch) const { return ch.dac_enabled != 0; }
    bool dac_enabled(const NoiseChannel& ch) const { return (ch.envelope & 0xF0) != 0; }

    // Sample generation
    float generate_pulse_sample(const PulseChannel& ch) const;
    float generate_wave_sample(const WaveChannel& ch) const;
    float generate_noise_sample(const NoiseChannel& ch) const;

    // Duty waveform
    uint8_t get_duty_waveform(uint8_t duty, uint8_t position) const;

    // Mix and push sample
    void mix_and_push_sample();

    // Debug helpers
    void debug_log(const char* op, uint16_t addr, uint8_t val) const {
        if (GB_APU_DEBUG) {
            std::cout << "[APU] " << op << " 0x" << std::hex << addr << " = 0x" << (int)val << std::dec << std::endl;
        }
    }

    uint8_t debug_read(uint16_t addr, uint8_t val) const {
        if (GB_APU_DEBUG) {
            std::cout << "[APU] RD 0x" << std::hex << addr << " -> 0x" << (int)val << std::dec << std::endl;
        }
        return val;
    }
};

#endif // APU_H