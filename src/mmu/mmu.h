#ifndef MMU_H
#define MMU_H

#include <cstdint>
#include <array>
#include <vector>
#include <string>
#include <fstream>
#include "ppu.h"
#include "apu.h"
#include "Timer.h"
#include "MBC.h"

class MMU {
public:
    MMU();
    ~MMU();
    // For debugging: set the current CPU PC so MMU can include it in write logs
    void set_last_cpu_pc(uint16_t pc) { last_cpu_pc = pc; }

    // Memory map
    uint8_t read_byte(uint16_t address);
    void write_byte(uint16_t address, uint8_t value);

    // ROM loading and parsing
    bool load_rom(const std::vector<uint8_t>& rom_data);
    void parse_rom_header();

    // ROM information
    std::string get_title() const { return rom_title; }
    std::string get_cartridge_type() const;
    std::string get_rom_size() const;
    std::string get_ram_size() const;
    bool has_battery() const;
    bool is_japanese() const;

    // PPU access
    PPU& get_ppu() { return ppu; }
    const PPU& get_ppu() const { return ppu; }
    // PPU 專用讀取：在 Mode2/3 期間也允許讀 VRAM/OAM（僅限 PPU 自身使用）
    uint8_t ppu_read(uint16_t address);

    // APU access
    APU& get_apu() { return apu; }

    // Joypad input handling
    void set_joypad_bit(int bit, bool pressed);
    uint8_t get_joypad_state(uint8_t select) const;

    // Timer access and update
    uint8_t get_timer_control() const { return timer->get_timer_control(); }
    void update_timer_cycles(uint8_t cycles);

    // Expose current T-cycle modulo (for quick timing hacks only)
    uint8_t get_cycle_mod4() const { return timer->get_cycle_mod4(); }

    // Force-align internal cycle counter to 4T boundary (Route A hack)
    void force_align_cycle_boundary() { timer->force_align_cycle_boundary(); }

private:
    std::array<uint8_t, 0x10000> memory; // 64KB total

    // Memory regions
    static constexpr uint16_t ROM_BANK_0_START = 0x0000;
    static constexpr uint16_t ROM_BANK_0_END = 0x3FFF;
    static constexpr uint16_t ROM_BANK_N_START = 0x4000;
    static constexpr uint16_t ROM_BANK_N_END = 0x7FFF;
    static constexpr uint16_t VRAM_START = 0x8000;
    static constexpr uint16_t VRAM_END = 0x9FFF;
    static constexpr uint16_t EXTERNAL_RAM_START = 0xA000;
    static constexpr uint16_t EXTERNAL_RAM_END = 0xBFFF;
    static constexpr uint16_t WRAM_START = 0xC000;
    static constexpr uint16_t WRAM_END = 0xDFFF;
    static constexpr uint16_t ECHO_RAM_START = 0xE000;
    static constexpr uint16_t ECHO_RAM_END = 0xFDFF;
    static constexpr uint16_t OAM_START = 0xFE00;
    static constexpr uint16_t OAM_END = 0xFE9F;
    static constexpr uint16_t IO_REGISTERS_START = 0xFF00;
    static constexpr uint16_t IO_REGISTERS_END = 0xFF7F;
    static constexpr uint16_t HRAM_START = 0xFF80;
    static constexpr uint16_t HRAM_END = 0xFFFE;
    static constexpr uint16_t INTERRUPT_ENABLE = 0xFFFF;

    // ROM data
    std::vector<uint8_t> rom;
    std::string rom_title;
    uint8_t cartridge_type;
    uint8_t rom_size_code;
    uint8_t ram_size_code;

    // PPU
    PPU ppu;

    // APU
    APU apu;

    // MBC (Memory Bank Controller) support
    MBC* mbc;
    std::vector<uint8_t> external_ram;

    // Interrupt registers
    uint8_t interrupt_flag;
    uint8_t interrupt_enable;

    // Timer
    Timer* timer;
    
    // Joypad state
    uint8_t joypad_state;

    // Serial output file
    std::ofstream serial_output_file;
    // For debug logging: track last known CPU PC
    uint16_t last_cpu_pc = 0;

    // Write byte helper functions
    bool handle_special_registers(uint16_t address, uint8_t value);
    bool handle_ppu_registers(uint16_t address, uint8_t value);
    bool handle_oam_access(uint16_t address, uint8_t value);
    bool handle_vram_access(uint16_t address, uint8_t value);
    bool handle_debug_output(uint16_t address, uint8_t value);
    bool handle_oam_dma(uint16_t address, uint8_t value);

    // --- OAM bug emulation state ---
    // Simplified model: when CPU writes to OAM during PPU mode 2/3, hardware bus
    // contention corrupts target bytes by duplicating the previous 2-byte pair.
    // We track last successfully written (unrestricted) OAM 2-byte pair start.
    uint16_t oam_bug_last_pair_base = 0xFE00; // start of last 2-byte pair
    bool oam_bug_last_valid = false;
};

#endif // MMU_H