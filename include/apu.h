#ifndef APU_H
#define APU_H

#include <cstdint>
#include <array>
#include <deque>
#include <iostream>
#include <SDL3/SDL.h>

#ifndef GB_APU_DEBUG
#define GB_APU_DEBUG 0
#endif

class APU {
public:
    APU();
    ~APU();

    void step(int cycles);
    void reset();

    // Register access
    uint8_t read_register(uint16_t address) const;
    void write_register(uint16_t address, uint8_t value);

    // Audio output
    void get_audio_samples(float* buffer, int length);

private:
    // Audio specifications
    static constexpr int SAMPLE_RATE = 44100; // Output sample rate
    static constexpr int CHANNELS = 2; // Stereo
    static constexpr float AMPLITUDE = 0.3f;
    static constexpr double CPU_CLOCK = 4194304.0; // Game Boy CPU clock (Hz)

    // Frame sequencer (512Hz)
    uint16_t frame_counter;
    uint8_t frame_step;
    bool apu_was_off = false; // Track power edge for NR52

    // Sample generation timing
    double sample_timer;              // Accumulated CPU cycles toward next audio sample
    double cycles_per_sample;         // CPU cycles per one audio sample (≈95.088)

    // FIFO of generated audio samples (interleaved stereo floats)
    std::deque<float> audio_fifo;

    // Channel 1: Pulse with sweep and envelope
    struct PulseChannel {
        // Registers
        uint8_t sweep;      // NR10 (0xFF10)
        uint8_t length;     // NR11 (0xFF11) - bits 6-5: duty, 0-5: length
        uint8_t envelope;   // NR12 (0xFF12)
        uint8_t frequency_lo; // NR13 (0xFF13)
        uint8_t frequency_hi; // NR14 (0xFF14) - bits 6-2: freq hi, 7: length enable, 6: trigger

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
    } ch1;

    struct PulseChannel ch2;

    struct WaveChannel {
        // Registers
        uint8_t dac_enabled;    // NR30 (0xFF1A) - bit 7: DAC enable
        uint8_t length;     // NR31 (0xFF1B)
        uint8_t volume;     // NR32 (0xFF1C) - bits 6-5: volume
        uint8_t frequency_lo; // NR33 (0xFF1D)
        uint8_t frequency_hi; // NR34 (0xFF1E) - bits 6-2: freq hi, 7: length enable, 6: trigger

        // Wave RAM (32 samples, 4-bit each)
        std::array<uint8_t, 16> wave_ram; // 0xFF30-0xFF3F

        // Internal state
        uint16_t frequency;
        uint8_t length_counter;
        uint8_t volume_shift;
        uint8_t enabled; // 統一命名
        uint16_t timer;
        uint8_t position;
        uint8_t sample_buffer;
    } ch3;

    struct NoiseChannel {
        // Registers
        uint8_t length;     // NR41 (0xFF20) - bits 5-0: length
        uint8_t envelope;   // NR42 (0xFF21)
        uint8_t polynomial; // NR43 (0xFF22)
        uint8_t control;    // NR44 (0xFF23) - bit 7: length enable, 6: trigger

        // Internal state
        uint8_t length_counter;
        uint8_t envelope_counter;
        uint8_t envelope_volume;
        uint8_t enabled;
        uint16_t timer;
        uint16_t lfsr;     // Linear Feedback Shift Register
    } ch4;

    // Sound control
    uint8_t nr50; // Master volume & VIN panning (0xFF24)
    uint8_t nr51; // Sound panning (0xFF25)
    uint8_t nr52; // Sound on/off (0xFF26)

    // Helper functions
    void update_frame_sequencer();
    void update_sweep(PulseChannel& ch);
    void update_envelope(PulseChannel& ch);
    void update_envelope(NoiseChannel& ch);
    void update_length(PulseChannel& ch);
    void update_length(WaveChannel& ch);
    void update_length(NoiseChannel& ch);

    float generate_pulse_sample(const PulseChannel& ch) const;
    float generate_wave_sample(const WaveChannel& ch) const;
    float generate_noise_sample(const NoiseChannel& ch) const;

    uint8_t get_duty_waveform(uint8_t duty, uint8_t position) const;

    // Mix current channel state into one stereo sample and push to fifo
    void mix_and_push_sample();
    void debug_log(const char* tag, uint16_t addr, uint8_t val) const {
#if GB_APU_DEBUG
        std::cout << "[APU] " << tag << " 0x" << std::hex << addr << " <= 0x" << (int)val << std::dec << std::endl;
#endif
    }
    void debug_read(uint16_t addr, uint8_t val) const {
#if GB_APU_DEBUG
        std::cout << "[APU] RD 0x" << std::hex << addr << " -> 0x" << (int)val << std::dec << std::endl;
#endif
    }
};

#endif // APU_H