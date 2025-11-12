#include "mmu.h"
#include <iostream>
#include <fstream>

MMU::MMU() : cartridge_type(0), rom_size_code(0), ram_size_code(0), interrupt_flag(0), interrupt_enable(0),
             mbc_type(MBC_NONE), mbc_ram_enabled(false), mbc_rom_bank(1), mbc_ram_bank(0), mbc_mode(0),
             divider(0), timer_counter(0), timer_control(0), internal_counter(0), timer_overflow_delay(0) {
    memory.fill(0);
}

MMU::~MMU() {
    // Cleanup if needed
}

uint8_t MMU::read_byte(uint16_t address) {
    // Handle memory-mapped I/O and special regions
    if (address <= ROM_BANK_0_END) {
        // ROM Bank 0 - always bank 0
        if (address < rom.size()) {
            return rom[address];
        } else {
            return 0xFF; // Default value
        }
    } else if (address >= ROM_BANK_N_START && address <= ROM_BANK_N_END) {
        // ROM Bank N - use MBC to determine which bank
        return get_mbc_rom_bank(address);
    } else if (address >= EXTERNAL_RAM_START && address <= EXTERNAL_RAM_END) {
        // External RAM - use MBC to determine which bank
        return get_mbc_ram_bank(address);
    } else if (address >= IO_REGISTERS_START && address <= IO_REGISTERS_END) {
        // I/O Registers - handle PPU, APU, and Timer registers
        switch (address) {
            case 0xFF00: return get_joypad_state(memory[0xFF00]);
            case 0xFF04: return (internal_counter >> 8) & 0xFF;  // DIV register (high byte of internal counter)
            case 0xFF05: return timer_counter;  // TIMA register
            case 0xFF07: return timer_control;  // TAC register
            case 0xFF0F: return interrupt_flag;
            // PPU registers
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
            // APU registers
            default: return apu.read_register(address);
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
        // ROM area - handle MBC writes
        handle_mbc_write(address, value);
        return;
    } else if (address >= EXTERNAL_RAM_START && address <= EXTERNAL_RAM_END) {
        // External RAM - only write if MBC RAM is enabled
        if (mbc_ram_enabled && !external_ram.empty()) {
            uint16_t ram_address = get_mbc_ram_address(address);
            if (ram_address < external_ram.size()) {
                external_ram[ram_address] = value;
            }
        }
        return;
    } else if (address >= IO_REGISTERS_START && address <= IO_REGISTERS_END) {
        // I/O Registers - handle PPU, APU, and Timer registers
        switch (address) {
            case 0xFF00: memory[0xFF00]=value; break; // Store select bits
            case 0xFF01: memory[0xFF01] = value; break; // Serial transfer data
            case 0xFF02: 
                memory[0xFF02] = value; 
                // Handle serial transfer start (bit 7 set)
                if (value & 0x80) {
                    static std::string serial_buffer;
                    char c = static_cast<char>(memory[0xFF01]);
                    if (c >= 32 || c == '\n' || c == '\r') {
                        std::cout << c << std::flush;
                        static std::ofstream serial_log("serial_output.txt", std::ios::app);
                        serial_log << c << std::flush;
                        serial_buffer += c;
                        // 若偵測到 "End" 字串，則強制結束程式
                        if (serial_buffer.size() >= 3 && serial_buffer.substr(serial_buffer.size()-3) == "End") {
                            std::cout << "\n[Emulator auto-exit: detected test end marker]\n" << std::endl;
                            serial_log << "\n[Emulator auto-exit: detected test end marker]\n" << std::endl;
                            serial_log.close();
                            std::exit(0);
                        }
                        // 避免 buffer 無限增長
                        if (serial_buffer.size() > 32) serial_buffer.erase(0, serial_buffer.size()-8);
                    }
                    // Trigger serial interrupt
                    interrupt_flag |= 0x08;
                }
                break;
            case 0xFF04: 
                // DIV write - always reset to 0
                // Disable glitch check for now
                divider = 0;
                internal_counter = 0;
                break;
            case 0xFF05: 
                // TIMA write
                timer_counter = value;
                // std::cout << "[TIMA Write] Set to " << (int)value << std::endl;
                break;
            case 0xFF07: 
                // TAC write
                set_tac(value);
                // std::cout << "[TAC Write] Set to " << std::hex << (int)value << std::dec << " (freq=" << (value & 0x03) << ", enable=" << ((value >> 2) & 1) << ")" << std::endl;
                break;
            case 0xFF0F: interrupt_flag = value; break;
            // PPU registers
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
            // APU registers
            default: apu.write_register(address, value); break;
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

    // Set MBC type based on cartridge type
    mbc_type = static_cast<MBCType>(cartridge_type);

    // Initialize external RAM if cartridge has RAM
    size_t ram_size = 0;
    switch (ram_size_code) {
        case 0x00: ram_size = 0; break;     // No RAM
        case 0x01: ram_size = 2048; break;  // 2KB
        case 0x02: ram_size = 8192; break;  // 8KB
        case 0x03: ram_size = 32768; break; // 32KB
        case 0x04: ram_size = 131072; break; // 128KB
        case 0x05: ram_size = 65536; break; // 64KB
        default: ram_size = 0; break;
    }

    if (ram_size > 0) {
        external_ram.resize(ram_size, 0);
        std::cout << "External RAM initialized: " << ram_size << " bytes" << std::endl;
    }

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

// MBC (Memory Bank Controller) implementation
void MMU::handle_mbc_write(uint16_t address, uint8_t value) {
    switch (mbc_type) {
        case MBC1:
        case MBC1_RAM:
        case MBC1_RAM_BATTERY:
            handle_mbc1_write(address, value);
            break;
        case MBC2:
        case MBC2_BATTERY:
            handle_mbc2_write(address, value);
            break;
        case MBC3:
        case MBC3_RAM:
        case MBC3_RAM_BATTERY:
        case MBC3_TIMER_BATTERY:
        case MBC3_TIMER_RAM_BATTERY:
            handle_mbc3_write(address, value);
            break;
        case MBC5:
        case MBC5_RAM:
        case MBC5_RAM_BATTERY:
        case MBC5_RUMBLE:
        case MBC5_RUMBLE_SRAM:
        case MBC5_RUMBLE_SRAM_BATTERY:
            handle_mbc5_write(address, value);
            break;
        default:
            // No MBC or unsupported MBC type
            break;
    }
}

void MMU::handle_mbc1_write(uint16_t address, uint8_t value) {
    if (address >= 0x0000 && address <= 0x1FFF) {
        // RAM enable/disable
        mbc_ram_enabled = ((value & 0x0F) == 0x0A);
    } else if (address >= 0x2000 && address <= 0x3FFF) {
        // ROM bank select (lower 5 bits)
        uint8_t bank = value & 0x1F;
        if (bank == 0) bank = 1; // Bank 0 is not allowed, use bank 1 instead
        mbc_rom_bank = (mbc_rom_bank & 0x60) | bank;
    } else if (address >= 0x4000 && address <= 0x5FFF) {
        // RAM bank select or upper ROM bank bits
        if (mbc_mode == 0) {
            // ROM mode: upper bits of ROM bank
            mbc_rom_bank = (mbc_rom_bank & 0x1F) | ((value & 0x03) << 5);
        } else {
            // RAM mode: RAM bank select
            mbc_ram_bank = value & 0x03;
        }
    } else if (address >= 0x6000 && address <= 0x7FFF) {
        // Mode select
        mbc_mode = value & 0x01;
    }
}

void MMU::handle_mbc2_write(uint16_t address, uint8_t value) {
    if (address >= 0x0000 && address <= 0x3FFF) {
        if (!(address & 0x0100)) { // RAM enable (even addresses)
            mbc_ram_enabled = ((value & 0x0F) == 0x0A);
        } else { // ROM bank select (odd addresses)
            uint8_t bank = value & 0x0F;
            if (bank == 0) bank = 1;
            mbc_rom_bank = bank;
        }
    }
}

void MMU::handle_mbc3_write(uint16_t address, uint8_t value) {
    if (address >= 0x0000 && address <= 0x1FFF) {
        // RAM/RTC enable
        mbc_ram_enabled = ((value & 0x0F) == 0x0A);
    } else if (address >= 0x2000 && address <= 0x3FFF) {
        // ROM bank select
        uint8_t bank = value & 0x7F;
        if (bank == 0) bank = 1;
        mbc_rom_bank = bank;
    } else if (address >= 0x4000 && address <= 0x5FFF) {
        // RAM bank select or RTC register select
        mbc_ram_bank = value;
    } else if (address >= 0x6000 && address <= 0x7FFF) {
        // Latch clock data (not implemented)
    }
}

void MMU::handle_mbc5_write(uint16_t address, uint8_t value) {
    if (address >= 0x0000 && address <= 0x1FFF) {
        // RAM enable
        mbc_ram_enabled = ((value & 0x0F) == 0x0A);
    } else if (address >= 0x2000 && address <= 0x2FFF) {
        // ROM bank select (lower 8 bits)
        mbc_rom_bank = (mbc_rom_bank & 0x0100) | value;
    } else if (address >= 0x3000 && address <= 0x3FFF) {
        // ROM bank select (9th bit)
        mbc_rom_bank = (mbc_rom_bank & 0x00FF) | ((value & 0x01) << 8);
    } else if (address >= 0x4000 && address <= 0x5FFF) {
        // RAM bank select
        mbc_ram_bank = value & 0x0F;
    }
}

uint8_t MMU::get_mbc_rom_bank(uint16_t address) const {
    uint16_t rom_address;
    uint16_t bank_offset = (mbc_rom_bank - 1) * 0x4000; // Each bank is 16KB
    rom_address = (address - ROM_BANK_N_START) + bank_offset + 0x4000;

    if (rom_address < rom.size()) {
        return rom[rom_address];
    } else {
        return 0xFF;
    }
}

uint8_t MMU::get_mbc_ram_bank(uint16_t address) const {
    if (!mbc_ram_enabled || external_ram.empty()) {
        return 0xFF;
    }

    uint16_t ram_address = get_mbc_ram_address(address);
    if (ram_address < external_ram.size()) {
        return external_ram[ram_address];
    } else {
        return 0xFF;
    }
}

uint16_t MMU::get_mbc_ram_address(uint16_t address) const {
    uint16_t ram_address = address - EXTERNAL_RAM_START;
    uint16_t bank_offset = mbc_ram_bank * 0x2000; // Each RAM bank is 8KB
    return ram_address + bank_offset;
}

void MMU::update_timer_cycles(uint8_t cycles) {
    // Handle TIMA overflow delay
    if (timer_overflow_delay > 0) {
        if (cycles >= timer_overflow_delay) {
            // Overflow delay expired - now set the interrupt flag
            interrupt_flag |= 0x04;
            timer_overflow_delay = 0;
        } else {
            timer_overflow_delay -= cycles;
            return;
        }
    }
    
    // The Game Boy's timer is driven by an internal 16-bit divider
    // that's always running
    
    uint16_t old_counter = internal_counter;
    internal_counter += cycles;
    internal_counter &= 0xFFFF;  // Keep it 16-bit
    
    // Check if timer is enabled (TAC bit 2)
    if (!(timer_control & 0x04)) {
        return;
    }
    
    // Select which bit to monitor based on TAC 0-1
    uint8_t freq_bits[] = { 9, 3, 5, 7 };
    uint8_t bit_position = freq_bits[timer_control & 0x03];
    
    // Check if the selected bit has changed from 1 to 0 (falling edge)
    uint16_t old_bit = (old_counter >> bit_position) & 1;
    uint16_t new_bit = (internal_counter >> bit_position) & 1;
    
    if (old_bit && !new_bit) {
        // Falling edge detected - increment TIMA
        if (timer_counter == 0xFF) {
            timer_counter = 0;
            // Set overflow delay instead of immediately setting interrupt
            timer_overflow_delay = 4;  // 4 M-cycles delay
        } else {
            timer_counter++;
        }
    }
}

void MMU::set_tac(uint8_t value) {
    // TAC bits: 7-2 unused, 1-0 = frequency select, 2 = enable
    
    // Disable glitch detection for now - just update the value
    timer_control = value & 0x07;
    
    // Note: The Game Boy has a known "glitch" where changing the frequency
    // or enable bit can cause TIMA to increment if a falling edge is detected
    // on the selected bit. This is complex and might be causing issues.
}
