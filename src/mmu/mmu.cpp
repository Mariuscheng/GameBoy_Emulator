// Clean timer-accurate MMU implementation

#include "mmu.h"
#include <iostream>
#include <algorithm>

// Serial debug flag
#ifndef GB_SERIAL_DEBUG
#define GB_SERIAL_DEBUG 1 // Enabled for capturing blargg test serial output
#endif

MMU::MMU()
    : cartridge_type(0), rom_size_code(0), ram_size_code(0),
      interrupt_flag(0), interrupt_enable(0),
      joypad_state(0xFF)
{
    timer = new Timer();
    mbc = nullptr;
    memory.fill(0xFF);
    // Initialize VRAM area to 0x00 to avoid default black tiles caused by 0xFF.
    // Many tests rely on VRAM being clear (0) until the ROM writes tiles into VRAM.
    for (uint16_t addr = 0x8000; addr <= 0x9FFF; ++addr) {
        memory[addr] = 0x00;
    }
    // Open serial output file in project root
    serial_output_file.open("serial_output.txt", std::ios::out | std::ios::trunc);
}

MMU::~MMU() {
    delete timer;
    delete mbc;
    // Close serial output file
    if (serial_output_file.is_open()) {
        serial_output_file.close();
    }
}

bool MMU::load_rom(const std::vector<uint8_t>& rom_data) {
    rom = rom_data;
    parse_rom_header();
    // Quick ROM scan for HALT (0x76) opcode occurrences to aid HALT bug debugging
    size_t halt_opcode_count = 0;
    std::vector<uint16_t> first_halt_addresses;
    for (size_t i = 0; i < rom.size(); ++i) {
        if (rom[i] == 0x76) {
            ++halt_opcode_count;
            if (first_halt_addresses.size() < 10) {
                first_halt_addresses.push_back(static_cast<uint16_t>(i));
            }
        }
    }
    // Map fixed bank 0 to memory for convenience (optional)
    size_t copy_len = std::min<size_t>(0x4000, rom.size());
    std::copy(rom.begin(), rom.begin() + copy_len, memory.begin());
    return true;
}

void MMU::parse_rom_header() {
    rom_title.clear();
    for (int i = 0x0134; i <= 0x0143 && i < (int)rom.size(); ++i) {
        char c = (char)rom[i];
        if (c == 0) break;
        rom_title += c;
    }
    if (rom.size() > 0x0149) {
        cartridge_type = rom[0x0147];
        rom_size_code = rom[0x0148];
        ram_size_code = rom[0x0149];
    }

    // Setup external RAM size
    size_t ram_size = 0;
    switch (ram_size_code) {
        case 0x00: ram_size = 0; break;
        case 0x01: ram_size = 2 * 1024; break;
        case 0x02: ram_size = 8 * 1024; break;
        case 0x03: ram_size = 32 * 1024; break;
        case 0x04: ram_size = 128 * 1024; break;
        case 0x05: ram_size = 64 * 1024; break;
        default: ram_size = 0; break;
    }
    external_ram.assign(ram_size, 0x00);

    // Create MBC
    mbc = new MBC(static_cast<MBCType>(cartridge_type), rom, external_ram);
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
    switch (cartridge_type) {
        case 0x03: case 0x06: case 0x09: case 0x0D: case 0x0F: case 0x10:
        case 0x13: case 0x1B: case 0x1E: case 0x22: case 0xFF:
            return true;
        default:
            return false;
    }
}

bool MMU::is_japanese() const {
    return rom.size() > 0x014A && rom[0x014A] == 0x00;
}

// --- Memory access ---

// PPU 專用讀取：繞過 Mode2/3 的 VRAM/OAM 鎖定，僅供 PPU 在渲染時讀取
uint8_t MMU::ppu_read(uint16_t address) {
    // VRAM 0x8000-0x9FFF、OAM 0xFE00-0xFE9F 直接讀取
    if ((address >= 0x8000 && address <= 0x9FFF) || (address >= 0xFE00 && address <= 0xFE9F)) {
        return memory[address];
    }
    // 其他位址沿用一般讀取（不會觸發 VRAM/OAM 鎖定）
    return read_byte(address);
}

uint8_t MMU::read_byte(uint16_t address) {
    if (address <= ROM_BANK_0_END) {
        if (address < rom.size()) return rom[address];
        return 0xFF;
    } else if (address >= ROM_BANK_N_START && address <= ROM_BANK_N_END) {
        // Switchable bank
        if (mbc) return mbc->get_rom_bank(address);
        return 0xFF;
    } else if (address >= EXTERNAL_RAM_START && address <= EXTERNAL_RAM_END) {
        if (mbc) return mbc->get_ram_bank(address);
        return 0xFF;
    } else if (address == 0xFF00) {
        return get_joypad_state(memory[0xFF00]);
    } else if (address == 0xFF04) {
        return timer->get_divider();
    } else if (address == 0xFF05) {
        return timer->get_timer_counter();
    } else if (address == 0xFF06) {
        return timer->get_timer_modulo();
    } else if (address == 0xFF07) {
        return timer->get_timer_control() | 0xF8; // upper bits read as 1
    } else if (address == 0xFF01) {
        return memory[0xFF01]; // Serial data register
    } else if (address == 0xFF02) {
        return memory[0xFF02]; // Serial control register
    } else if (address == 0xFF0F) {
        return (interrupt_flag | 0xE0); // upper 3 bits typically read as 1 on DMG
    } else if (address == 0xFFFF) {
        return interrupt_enable;
    }

    // PPU registers
    switch (address) {
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
        default: break;
    }

    // APU registers
    if (address >= 0xFF10 && address <= 0xFF3F) {
        // Before reading NR52, make sure APU flags reflect current counters/DAC
        if (address == 0xFF26) { apu.flush_for_nr52_read(); }
        return apu.read_register(address);
    }

    // OAM access restriction during Mode 2 (OAM search) and Mode 3 (pixel transfer)
    if (address >= 0xFE00 && address <= 0xFE9F) {
        uint8_t mode = ppu.get_stat() & 0x03;
        if (mode == 2 || mode == 3) {
            return 0xFF; // OAM locked, return 0xFF
        }
    }

    // VRAM access restriction during Mode 3 (pixel transfer)
    if (address >= 0x8000 && address <= 0x9FFF) {
        uint8_t mode = ppu.get_stat() & 0x03;
        if (mode == 3) {
            return 0xFF; // VRAM locked, return 0xFF
        }
    }

    return memory[address];
}

void MMU::write_byte(uint16_t address, uint8_t value) {
    if (address <= ROM_BANK_N_END) {
        // MBC writes
        if (mbc) mbc->handle_write(address, value);
        return;
    }

    // Handle special registers (joypad, serial, timer, interrupts)
    if (handle_special_registers(address, value)) return;

    // Handle PPU registers
    if (handle_ppu_registers(address, value)) return;

    // APU IO
    if (address >= 0xFF10 && address <= 0xFF3F) { apu.write_register(address, value); return; }

    // Handle OAM access with corruption emulation
    if (handle_oam_access(address, value)) return;

    // Handle VRAM access with restrictions
    if (handle_vram_access(address, value)) return;

    // Handle debug output
    if (handle_debug_output(address, value)) return;

    // Handle OAM DMA
    if (handle_oam_dma(address, value)) return;

    memory[address] = value;
}

bool MMU::handle_special_registers(uint16_t address, uint8_t value) {
    switch (address) {
        case 0xFF00:
            memory[0xFF00] = value; // joypad select bits
            return true;
        case 0xFF01: {
            memory[0xFF01] = value; // Serial data register
#if GB_SERIAL_DEBUG
            // Print test output for serial debugging
            char output_char = (char)value;
            std::cout << output_char;
            std::cout.flush();
            // Also write to serial output file
            if (serial_output_file.is_open()) {
                serial_output_file << output_char;
                serial_output_file.flush();
            }
#endif
            return true;
        }
        case 0xFF02:
            memory[0xFF02] = value; // Serial control register
            return true;
        case 0xFF04:
            timer->set_divider(0);
            return true;
        case 0xFF05: // TIMA
            timer->set_timer_counter(value);
            return true;
        case 0xFF06: // TMA
            timer->set_timer_modulo(value);
            return true;
        case 0xFF07: // TAC
            timer->set_tac(value);
            return true;
        case 0xFF0F: // IF
            interrupt_flag = value & 0x1F;  // Only bits 0-4 are writable
            return true;
        case 0xFFFF: // IE
            interrupt_enable = value;
            return true;
        default:
            return false;
    }
}

bool MMU::handle_ppu_registers(uint16_t address, uint8_t value) {
    switch (address) {
        case 0xFF40: ppu.set_lcdc(value); return true;
        case 0xFF41: ppu.set_stat(value); return true;
        case 0xFF42: ppu.set_scy(value); return true;
        case 0xFF43: ppu.set_scx(value); return true;
        case 0xFF44: ppu.set_ly(0); return true; // Writing to LY resets it to 0 on hardware
        case 0xFF45: ppu.set_lyc(value); return true;
        case 0xFF47: ppu.set_bgp(value); return true;
        case 0xFF48: ppu.set_obp0(value); return true;
        case 0xFF49: ppu.set_obp1(value); return true;
        case 0xFF4A: ppu.set_wy(value); return true;
        case 0xFF4B: ppu.set_wx(value); return true;
        default: return false;
    }
}

bool MMU::handle_oam_access(uint16_t address, uint8_t value) {
    if (address >= 0xFE00 && address <= 0xFE9F) {
        uint8_t mode = ppu.get_stat() & 0x03;
        if (mode == 2) {
            // OAM bug: During Mode 2 (OAM Search), writes to OAM are corrupted.
            // The written value is replaced with the byte currently being read by PPU's OAM search.
            // Mode 2: PPU scans sprites sequentially (2 cycles per sprite: Y then X)
            // - Even cycles (0,2,4...): reading sprite Y (offset +0)
            // - Odd cycles (1,3,5...): reading sprite X (offset +1)
            
            uint16_t source_base = ppu.get_oam_search_pair_base();
            
            // Determine which byte (Y or X) is currently being read based on PPU cycle phase
            uint8_t cycle_mod = ppu.get_cycle_mod4() & 0x01; // 0 = Y, 1 = X
            uint16_t source_addr = source_base + cycle_mod;
            
            // Write the corrupted value (single byte from PPU's current read position)
            uint8_t corrupted_value = memory[source_addr];
            memory[address] = corrupted_value;
            return true; // write consumed by corruption
        } else if (mode == 3) {
            return true; // OAM locked during Mode 3 (Pixel Transfer), ignore write
        }
        // Normal (unrestricted) OAM write for other modes - continue to normal write
    }
    return false;
}

bool MMU::handle_vram_access(uint16_t address, uint8_t value) {
    if (address >= 0x8000 && address <= 0x9FFF) {
        uint8_t mode = ppu.get_stat() & 0x03;
        if (mode == 3) {
            return true; // VRAM locked, ignore write
        }
    }
    return false;
}

bool MMU::handle_debug_output(uint16_t address, uint8_t value) {
    // Debug function - removed debug output
    return false;
}

bool MMU::handle_oam_dma(uint16_t address, uint8_t value) {
    if (address == 0xFF46) {
        // On real hardware, DMA takes ~160 µs (160 bytes) and blocks CPU on DMG.
        // For now, perform an immediate copy for functional correctness.
        uint16_t src_base = static_cast<uint16_t>(value) << 8; // value * 0x100
        for (uint16_t i = 0; i < 160; ++i) {
            uint16_t src = src_base + i;
            uint16_t dst = 0xFE00 + i;
            // During DMA, reads should come from memory array directly (bypass restrictions)
            uint8_t b = memory[src];
            memory[dst] = b;
        }
        memory[address] = value;
        return true;
    }
    return false;
}

// Debug toggle for timer logs
#ifndef GB_DEBUG_TIMER
#define GB_DEBUG_TIMER 0
#endif

void MMU::update_timer_cycles(uint8_t cycles) {
    bool interrupt = timer->update_cycles(cycles);
    if (interrupt) {
        interrupt_flag |= 0x04;
    }
}

// --- Joypad ---
void MMU::set_joypad_bit(int bit, bool pressed) {
    if (pressed) {
        // Active low
        joypad_state &= ~(1 << bit);
    } else {
        joypad_state |= (1 << bit);
    }
}

uint8_t MMU::get_joypad_state(uint8_t select) const {
    // Select bits: bit4=direction (0 active), bit5=buttons (0 active)
    uint8_t result = 0xFF;
    // Preserve upper bits
    result = (result & 0xF0);
    if (!(select & 0x10)) { // Direction
        result = (result & 0xF0) | (joypad_state & 0x0F);
    }
    if (!(select & 0x20)) { // Buttons
        result = (result & 0xF0) | ((joypad_state >> 4) & 0x0F);
    }
    // Bits 6-7 stay high
    result |= 0xC0;
    return result;
}


