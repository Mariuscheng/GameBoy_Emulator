#ifndef MMU_H
#define MMU_H

#include <cstdint>
#include <array>
#include <vector>
#include <string>
#include "ppu.h"
#include "apu.h"

class MMU {
public:
    MMU();
    ~MMU();

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

    // APU access
    APU& get_apu() { return apu; }

    // Joypad input handling
    void set_joypad_bit(int bit, bool pressed);
    uint8_t get_joypad_state(uint8_t select) const;

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

    // Interrupt registers
    uint8_t interrupt_flag; // IF (0xFF0F)
    uint8_t interrupt_enable; // IE (0xFFFF)

    // Joypad state
    uint8_t joypad_state = 0xFF; // 8 bits: 0=pressed, 1=released
};

#endif // MMU_H