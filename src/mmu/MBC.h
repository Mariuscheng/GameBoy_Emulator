#pragma once

#include <cstdint>
#include <vector>

enum MBCType {
    MBC_NONE = 0x00,
    MBC1 = 0x01,
    MBC1_RAM = 0x02,
    MBC1_RAM_BATTERY = 0x03,
    MBC2 = 0x05,
    MBC2_BATTERY = 0x06,
    MBC3 = 0x0F,
    MBC3_TIMER_BATTERY = 0x10,
    MBC3_RAM = 0x11,
    MBC3_RAM_BATTERY = 0x12,
    MBC3_TIMER_RAM_BATTERY = 0x13,
    MBC5 = 0x19,
    MBC5_RAM = 0x1A,
    MBC5_RAM_BATTERY = 0x1B,
    MBC5_RUMBLE = 0x1C,
    MBC5_RUMBLE_SRAM = 0x1D,
    MBC5_RUMBLE_SRAM_BATTERY = 0x1E
};

class MBC {
public:
    MBC(MBCType type, const std::vector<uint8_t>& rom, std::vector<uint8_t>& external_ram);
    ~MBC() = default;

    void handle_write(uint16_t address, uint8_t value);
    uint8_t get_rom_bank(uint16_t address) const;
    uint8_t get_ram_bank(uint16_t address) const;

    bool is_ram_enabled() const { return mbc_ram_enabled; }

private:
    MBCType mbc_type;
    bool mbc_ram_enabled;
    uint16_t mbc_rom_bank;
    uint8_t mbc_ram_bank;
    uint8_t mbc_mode;

    const std::vector<uint8_t>& rom;
    std::vector<uint8_t>& external_ram;

    void handle_mbc1_write(uint16_t address, uint8_t value);
    void handle_mbc2_write(uint16_t address, uint8_t value);
    void handle_mbc3_write(uint16_t address, uint8_t value);
    void handle_mbc5_write(uint16_t address, uint8_t value);

    uint16_t get_ram_address(uint16_t address) const;
};