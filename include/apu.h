#ifndef APU_H
#define APU_H

#include <array>
#include <deque>
#include <cstdint>
#include <iostream>

// Debug macro for APU
#ifndef GB_APU_DEBUG
#define GB_APU_DEBUG 1
#endif

// APU Register Addresses
#define NR10 0xFF10
#define NR11 0xFF11
#define NR12 0xFF12
#define NR13 0xFF13
#define NR14 0xFF14
#define NR21 0xFF16
#define NR22 0xFF17
#define NR23 0xFF18
#define NR24 0xFF19
#define NR30 0xFF1A
#define NR31 0xFF1B
#define NR32 0xFF1C
#define NR33 0xFF1D
#define NR34 0xFF1E
#define NR41 0xFF20
#define NR42 0xFF21
#define NR43 0xFF22
#define NR44 0xFF23
#define NR50 0xFF24
#define NR51 0xFF25
#define NR52 0xFF26

class APU {
public:
    APU();
    ~APU();

    void reset();
    void step(int cycles);
    uint8_t read_register(uint16_t address) const;
    void write_register(uint16_t address, uint8_t value);

    void get_audio_samples(float* buffer, int length);
    void handle_side_effects(uint16_t address, uint8_t value, uint8_t old_reg);

private:
    // Register descriptor for table-driven APU register behavior
    struct RegisterDescriptor {
        uint8_t read_mask;          // Bits readable as actual value
        uint8_t write_mask;         // Bits writable when writing
        bool    writable_when_off;  // Whether writable when APU power is off
        uint8_t default_read;       // Value returned for unreadable bits
    };

    // Constants
    static constexpr int CPU_CLOCK = 4194304;
    static constexpr int SAMPLE_RATE = 44100;
    static constexpr float AMPLITUDE = 0.1f;

    // Frame sequencer constants
    static constexpr int FRAME_SEQUENCER_PERIOD = 8192; // 512Hz

    // Flat APU register storage for 0xFF10-0xFF26 (0x17 bytes)
    std::array<uint8_t, 0x17> regs{};

    // Wave RAM (0xFF30-0xFF3F), accessible regardless of power state
    std::array<uint8_t, 16> wave_ram{};

    struct PulseChannel {
        // === 寄存器快照（NRx0 - NRx4）===
        uint8_t sweep;         // NR10 - sweep
        uint8_t length;        // NR11 - duty + length
        uint8_t envelope;      // NR12 - envelope
        uint8_t frequency_lo;  // NR13 - freq low
        uint8_t frequency_hi;  // NR14 - freq hi + control

        // === 內部狀態（從寄存器解析）===
        uint16_t frequency = 0;           // 11-bit frequency
        uint8_t  duty = 0;                // 0-3, from NR11/NR21 bits 6-7
        uint16_t length_counter = 0;      // 0~63 (ch1/ch2), 0~255 (ch3), 0~63 (ch4)
        bool     length_enabled = false;  // NRx4 bit6

        // === 包絡 (Envelope) ===
        uint8_t  envelope_volume = 0;     // 0~15
        bool     envelope_increase = false;
        uint8_t  envelope_period = 0;
        uint8_t  envelope_counter = 0;

        // === 掃描 (Sweep) - 僅 ch1 使用 ===
        uint8_t  sweep_period = 0;
        bool     sweep_direction = false; // false=add, true=subtract
        uint8_t  sweep_shift = 0;
        uint8_t  sweep_counter = 0;
        bool     sweep_enabled = false;
        uint16_t sweep_frequency = 0;     // shadow register

        // === 執行狀態 ===
        bool     enabled = false;         // 是否正在運行
        uint16_t timer = 0;               // 週期計時器
        uint8_t  position = 0;            // 0~7 (duty cycle position)
    };

    struct WaveChannel {
        uint8_t dac_enable;       // NR30
        uint8_t length;           // NR31
        uint8_t volume_code;      // NR32
        uint8_t frequency_lo;     // NR33
        uint8_t frequency_hi;     // NR34

        uint16_t frequency = 0;
        uint16_t length_counter = 0;
        bool     length_enabled = false;
        uint8_t  sample_buffer = 0;
            // wave RAM removed from channel, use unified APU::wave_ram array
        bool     enabled = false;
        uint16_t timer = 0;
        uint8_t  position = 0;    // 0~31
    };

    struct NoiseChannel {
        uint8_t length;           // NR41
        uint8_t envelope;         // NR42
        uint8_t polynomial;       // NR43
        uint8_t control;          // NR44

        uint16_t length_counter = 0;
        bool     length_enabled = false;
        uint8_t  envelope_volume = 0;
        bool     envelope_increase = false;
        uint8_t  envelope_period = 0;
        uint8_t  envelope_counter = 0;
        uint16_t lfsr = 0x7FFF;
        bool     enabled = false;
        uint16_t timer = 0;
    };

    // Channels
    PulseChannel ch1;
    PulseChannel ch2;
    WaveChannel ch3;
    NoiseChannel ch4;

    // Lightweight status per channel used by register logic
    struct ChannelState {
        bool enabled{false};
        bool dac_on{false};
        int  timer{0};
    };

    ChannelState ch1_state;
    ChannelState ch2_state;
    ChannelState ch3_state;
    ChannelState ch4_state;

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
    void update_length(int channel_num);

    // Timer update helpers
    void update_pulse_timer(PulseChannel& ch, int cycles);
    void update_wave_timer(WaveChannel& ch, int cycles);

    // Unified channel trigger
    void trigger_channel(PulseChannel& ch, uint8_t length_reg, bool is_ch1 = false);
    void trigger_channel(WaveChannel& ch);
    void trigger_channel(NoiseChannel& ch);

    // Simplified helpers used by current implementation in apu.cpp
    bool apu_powered() const { return (regs[0x16] & 0x80) != 0; }
    uint8_t get_channel_status() const;
    void power_off_apu();
    void power_on_apu();
    void update_dac_state(int channel_num);
    void update_dac_state(uint16_t address, uint8_t value);
    void trigger_channel(int channel_num);

    // Sub-functions for trigger_channel to reduce complexity
    void initialize_channel_state(int channel_num);
    void reload_length_counter(int channel_num);
    void initialize_sweep(int channel_num);
    void initialize_envelope(int channel_num);
    void reset_timers_and_phase(int channel_num);
    void check_immediate_sweep_overflow(int channel_num);

    // Handle length and trigger logic for NR14/NR24/NR34/NR44
    void handle_length_trigger(uint16_t address, uint8_t value, uint8_t old_reg, int channel_num);

    // Additional register write helpers
    void handle_sweep(uint8_t value);
    void handle_duty_length(int channel_num, uint8_t value);
    void handle_envelope(int channel_num, uint8_t value);
    void handle_wave_length(uint8_t value);
    void handle_noise_length(uint8_t value);
    void handle_frequency_low(int channel_num, uint8_t value);
    void handle_wave_on_off(uint8_t value);
    uint8_t read_wave_ram(uint16_t address) const;
    void write_wave_ram(uint16_t address, uint8_t value);

    // Sub-functions for handle_side_effects to reduce complexity
    void handle_nr11_side_effects(uint8_t value);
    void handle_nr21_side_effects(uint8_t value);
    void handle_nr31_side_effects(uint8_t value);
    void handle_nr41_side_effects(uint8_t value);

    // Minimal sync used before NR52 reads to ensure flags are consistent
public:
    void flush_for_nr52_read();

    // DAC check
    bool dac_enabled(const PulseChannel& ch) const { return (ch.envelope & 0xF0) != 0; }
    bool dac_enabled(const WaveChannel& ch) const { return ch.dac_enable != 0; }
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
    // When true, only log length-related events (useful for Blargg tests)
    bool debug_len_only{false};
    // Unit test helpers (debug only)
    void debug_set_frame_step(int fs) { frame_step = fs; }
    void debug_set_length_counter(int channel, int value) {
        switch(channel) {
            case 1: ch1.length_counter = value; break;
            case 2: ch2.length_counter = value; break;
            case 3: ch3.length_counter = value; break;
            case 4: ch4.length_counter = value; break;
        }
    }
    void debug_set_reg(uint16_t address, uint8_t value) { if (address >= 0xFF10 && address <= 0xFF26) regs[address - 0xFF10] = value; }
    int debug_get_length_counter(int channel) const {
        switch(channel) {
            case 1: return ch1.length_counter;
            case 2: return ch2.length_counter;
            case 3: return ch3.length_counter;
            case 4: return ch4.length_counter;
            default: return -1;
        }
    }
    int debug_get_frame_counter() const { return frame_counter; }
    int debug_get_frame_step() const { return frame_step; }
    void debug_log(const char* op, uint16_t addr, uint8_t val) const {
        if (GB_APU_DEBUG && !this->debug_len_only) {
            std::cout << "[APU] " << op << " 0x" << std::hex << addr << " = 0x" << (int)val << std::dec << std::endl;
        }
    }

    uint8_t debug_read(uint16_t addr, uint8_t val) const {
        if (GB_APU_DEBUG && !this->debug_len_only) {
            std::cout << "[APU] RD 0x" << std::hex << addr << " -> 0x" << (int)val << std::dec << std::endl;
        }
        return val;
    }

    // Enable/disable the len-only debug filter
    void set_debug_len_only(bool v) { debug_len_only = v; }

    // Register table declaration (defined in apu.cpp)
    static const RegisterDescriptor reg_table[0x17];
};

#endif // APU_H