#include "MBC.h"

MBC::MBC(MBCType type, const std::vector<uint8_t>& rom_ref, std::vector<uint8_t>& external_ram_ref)
    : mbc_type(type), mbc_ram_enabled(false), mbc_rom_bank(1), mbc_ram_bank(0), mbc_mode(0),
      rom(rom_ref), external_ram(external_ram_ref)
{
}

void MBC::handle_write(uint16_t address, uint8_t value) {
    switch (mbc_type) {
        case MBC1:
        case MBC1_RAM:
        case MBC1_RAM_BATTERY:
            handle_mbc1_write(address, value); break;
        case MBC2:
        case MBC2_BATTERY:
            handle_mbc2_write(address, value); break;
        case MBC3:
        case MBC3_RAM:
        case MBC3_RAM_BATTERY:
        case MBC3_TIMER_BATTERY:
        case MBC3_TIMER_RAM_BATTERY:
            handle_mbc3_write(address, value); break;
        case MBC5:
        case MBC5_RAM:
        case MBC5_RAM_BATTERY:
        case MBC5_RUMBLE:
        case MBC5_RUMBLE_SRAM:
        case MBC5_RUMBLE_SRAM_BATTERY:
            handle_mbc5_write(address, value); break;
        default:
            // ROM ONLY: ignore
            break;
    }
}

uint8_t MBC::get_rom_bank(uint16_t address) const {
    uint32_t bank = mbc_rom_bank ? mbc_rom_bank : 1;
    uint32_t base = bank * 0x4000u;
    uint32_t idx = base + (address - 0x4000);
    if (idx < rom.size()) return rom[idx];
    return 0xFF;
}

uint8_t MBC::get_ram_bank(uint16_t address) const {
    if (!mbc_ram_enabled || external_ram.empty()) return 0xFF;
    uint16_t ram_address = get_ram_address(address);
    if (ram_address < external_ram.size()) return external_ram[ram_address];
    return 0xFF;
}

void MBC::handle_mbc1_write(uint16_t address, uint8_t value) {
    if (address >= 0x2000 && address <= 0x3FFF) {
        uint8_t bank = value & 0x1F; if (bank == 0) bank = 1; mbc_rom_bank = (mbc_rom_bank & 0x60) | bank;
    } else if (address >= 0x4000 && address <= 0x5FFF) {
        if (mbc_mode == 0) { mbc_rom_bank = (mbc_rom_bank & 0x1F) | ((value & 0x03) << 5); }
        else { mbc_ram_bank = value & 0x03; }
    } else if (address >= 0x6000 && address <= 0x7FFF) {
        mbc_mode = value & 0x01;
    }
}

void MBC::handle_mbc2_write(uint16_t address, uint8_t value) {
    if (address >= 0x2000 && address <= 0x3FFF) {
        uint8_t bank = value & 0x0F; if (bank == 0) bank = 1; mbc_rom_bank = bank;
    }
}

void MBC::handle_mbc3_write(uint16_t address, uint8_t value) {
    if (address >= 0x2000 && address <= 0x3FFF) {
        uint8_t bank = value & 0x7F; if (bank == 0) bank = 1; mbc_rom_bank = bank;
    } else if (address >= 0x4000 && address <= 0x5FFF) {
        mbc_ram_bank = value;
    }
}

void MBC::handle_mbc5_write(uint16_t address, uint8_t value) {
    if (address >= 0x2000 && address <= 0x2FFF) {
        mbc_rom_bank = (mbc_rom_bank & 0x0100) | value;
    } else if (address >= 0x3000 && address <= 0x3FFF) {
        mbc_rom_bank = (mbc_rom_bank & 0x00FF) | ((value & 0x01) << 8);
    }
}

uint16_t MBC::get_ram_address(uint16_t address) const {
    uint16_t bank_offset = mbc_ram_bank * 0x2000; // 8KB per bank
    return (address - 0xA000) + bank_offset;
}