#include "mmu.h"
#include <iostream>

MMU::MMU() : cartridge_type(0), rom_size_code(0), ram_size_code(0), interrupt_flag(0), interrupt_enable(0) {
    memory.fill(0);
}

MMU::~MMU() {
    // Cleanup if needed
}

uint8_t MMU::read_byte(uint16_t address) {
    // Handle memory-mapped I/O and special regions
    if (address <= ROM_BANK_0_END) {
        // ROM Bank 0
        if (address < rom.size()) {
            return rom[address];
        } else {
            return 0xFF; // Default value
        }
    } else if (address >= ROM_BANK_N_START && address <= ROM_BANK_N_END) {
        // ROM Bank N (for now, same as Bank 0)
        uint16_t rom_address = address - ROM_BANK_N_START + 0x4000;
        if (rom_address < rom.size()) {
            return rom[rom_address];
        } else {
            return 0xFF;
        }
    } else if (address >= IO_REGISTERS_START && address <= IO_REGISTERS_END) {
        // I/O Registers - handle PPU registers
        switch (address) {
            case 0xFF00: return get_joypad_state(memory[0xFF00]);
            case 0xFF0F: return interrupt_flag;
            case 0xFF40: return ppu.get_lcdc();
            case 0xFF41: return ppu.get_stat();
            case 0xFF42: return ppu.get_scy();
            case 0xFF43: return ppu.get_scx();
            case 0xFF44: return ppu.get_ly();
            case 0xFF45: return ppu.get_lyc();
            case 0xFF47: return ppu.get_bgp();
            case 0xFF48: return ppu.get_obp0();
            case 0xFF49: return ppu.get_obp1();
            case 0xFF4A: return ppu.get_wy();
            case 0xFF4B: return ppu.get_wx();
            default: return memory[address];
        }
    } else if (address == 0xFFFF) {
        return interrupt_enable;
    } else {
        // Other memory regions
        return memory[address];
    }
}

void MMU::write_byte(uint16_t address, uint8_t value) {
    // Handle writes to different memory regions
    if (address >= ROM_BANK_0_START && address <= ROM_BANK_N_END) {
        // ROM is read-only, ignore writes
        return;
    } else if (address >= IO_REGISTERS_START && address <= IO_REGISTERS_END) {
        // I/O Registers - handle PPU registers
        switch (address) {
            case 0xFF00: memory[0xFF00]=value; break; // Store select bits
            case 0xFF0F: interrupt_flag = value; break;
            case 0xFF40: ppu.set_lcdc(value); break;
            case 0xFF41: ppu.set_stat(value); break;
            case 0xFF42: ppu.set_scy(value); break;
            case 0xFF43: ppu.set_scx(value); break;
            case 0xFF44: /* LY is read-only */ break;
            case 0xFF45: ppu.set_lyc(value); break;
            case 0xFF47: ppu.set_bgp(value); break;
            case 0xFF48: ppu.set_obp0(value); break;
            case 0xFF49: ppu.set_obp1(value); break;
            case 0xFF4A: ppu.set_wy(value); break;
            case 0xFF4B: ppu.set_wx(value); break;
            default: memory[address] = value; break;
        }
        return;
    } else if (address == 0xFFFF) {
        interrupt_enable = value;
        return;
    } else if (address >= ECHO_RAM_START && address <= ECHO_RAM_END) {
        // Echo RAM mirrors WRAM
        memory[address] = value;
        memory[address - 0x2000] = value; // Mirror to WRAM
    } else {
        memory[address] = value;
    }
}

bool MMU::load_rom(const std::vector<uint8_t>& rom_data) {
    if (rom_data.size() < 0x150) { // Minimum ROM size with header
        std::cout << "ROM file too small" << std::endl;
        return false;
    }

    rom = rom_data;
    parse_rom_header();
    return true;
}

void MMU::parse_rom_header() {
    // Parse title (0x0134-0x0143)
    rom_title = "";
    for (int i = 0x0134; i <= 0x0143; ++i) {
        char c = rom[i];
        if (c == 0) break;
        rom_title += c;
    }

    // Cartridge type (0x0147)
    cartridge_type = rom[0x0147];

    // ROM size (0x0148)
    rom_size_code = rom[0x0148];

    // RAM size (0x0149)
    ram_size_code = rom[0x0149];

    // Destination code (0x014A) - 0x00 = Japanese, 0x01 = Worldwide
    // Licensee code (0x0144-0x0145) for newer games, or old licensee (0x014B)

    std::cout << "ROM Title: " << rom_title << std::endl;
    std::cout << "Cartridge Type: " << get_cartridge_type() << std::endl;
    std::cout << "ROM Size: " << get_rom_size() << std::endl;
    std::cout << "RAM Size: " << get_ram_size() << std::endl;
    std::cout << "Has Battery: " << (has_battery() ? "Yes" : "No") << std::endl;
    std::cout << "Region: " << (is_japanese() ? "Japanese" : "Worldwide") << std::endl;
}

std::string MMU::get_cartridge_type() const {
    switch (cartridge_type) {
        case 0x00: return "ROM ONLY";
        case 0x01: return "MBC1";
        case 0x02: return "MBC1+RAM";
        case 0x03: return "MBC1+RAM+BATTERY";
        case 0x05: return "MBC2";
        case 0x06: return "MBC2+BATTERY";
        case 0x08: return "ROM+RAM";
        case 0x09: return "ROM+RAM+BATTERY";
        case 0x0B: return "MMM01";
        case 0x0C: return "MMM01+RAM";
        case 0x0D: return "MMM01+RAM+BATTERY";
        case 0x0F: return "MBC3+TIMER+BATTERY";
        case 0x10: return "MBC3+TIMER+RAM+BATTERY";
        case 0x11: return "MBC3";
        case 0x12: return "MBC3+RAM";
        case 0x13: return "MBC3+RAM+BATTERY";
        case 0x19: return "MBC5";
        case 0x1A: return "MBC5+RAM";
        case 0x1B: return "MBC5+RAM+BATTERY";
        case 0x1C: return "MBC5+RUMBLE";
        case 0x1D: return "MBC5+RUMBLE+RAM";
        case 0x1E: return "MBC5+RUMBLE+RAM+BATTERY";
        case 0x20: return "MBC6";
        case 0x22: return "MBC7+SENSOR+RUMBLE+RAM+BATTERY";
        case 0xFC: return "POCKET CAMERA";
        case 0xFD: return "BANDAI TAMA5";
        case 0xFE: return "HuC3";
        case 0xFF: return "HuC1+RAM+BATTERY";
        default: return "UNKNOWN";
    }
}

std::string MMU::get_rom_size() const {
    switch (rom_size_code) {
        case 0x00: return "32KB";
        case 0x01: return "64KB";
        case 0x02: return "128KB";
        case 0x03: return "256KB";
        case 0x04: return "512KB";
        case 0x05: return "1MB";
        case 0x06: return "2MB";
        case 0x07: return "4MB";
        case 0x08: return "8MB";
        case 0x52: return "1.1MB";
        case 0x53: return "1.2MB";
        case 0x54: return "1.5MB";
        default: return "UNKNOWN";
    }
}

std::string MMU::get_ram_size() const {
    switch (ram_size_code) {
        case 0x00: return "None";
        case 0x01: return "2KB";
        case 0x02: return "8KB";
        case 0x03: return "32KB";
        case 0x04: return "128KB";
        case 0x05: return "64KB";
        default: return "UNKNOWN";
    }
}

bool MMU::has_battery() const {
    // Cartridge types with battery backup
    return cartridge_type == 0x03 || cartridge_type == 0x06 || cartridge_type == 0x09 ||
           cartridge_type == 0x0D || cartridge_type == 0x0F || cartridge_type == 0x10 ||
           cartridge_type == 0x13 || cartridge_type == 0x1B || cartridge_type == 0x1E ||
           cartridge_type == 0x22 || cartridge_type == 0xFF;
}

bool MMU::is_japanese() const {
    return rom[0x014A] == 0x00;
}

void MMU::set_joypad_bit(int bit, bool pressed) {
    if (pressed)
        joypad_state &= ~(1 << bit);
    else
        joypad_state |= (1 << bit);
}

uint8_t MMU::get_joypad_state(uint8_t select) const {
    // select:0x10=direction,0x20=button
    uint8_t result = 0xCF;
    if (!(select & 0x10)) {
        // Direction keys: Down, Up, Left, Right (bits3-0)
        result &= (0xF0 | (joypad_state & 0x0F));
    } else if (!(select & 0x20)) {
        // Button keys: Start, Select, B, A (bits3-0)
        result &= (0xF0 | ((joypad_state >> 4) & 0x0F));
    }
    return result;
}