#ifndef EMULATOR_H
#define EMULATOR_H

#include "cpu.h"
#include "mmu.h"
#include <SDL3/SDL.h>
#include <vector>
#include <string>

class Emulator {
public:
    Emulator();
    ~Emulator();

    bool initialize();
    bool load_rom(const std::string& rom_path);
    void run();
    void shutdown();

    // Expose PPU LCD start offset configuration for timing experiments
    void set_ppu_lcd_start_offset(uint16_t offset);

    // Optional: limit how many frames to run before exiting (0 = unlimited)
    void set_max_frames(int frames) { max_frames = frames; }

    // Save current framebuffer to a simple binary PPM file (P6)
    bool save_framebuffer_ppm(const std::string& path) const;

    // Set headless mode (no SDL window, for testing)
    void set_headless(bool headless) { this->headless = headless; }
    // Expose APU access for unit tests/debugging
    APU& debug_get_apu() { return mmu.get_apu(); }
    // Expose PPU access for unit tests/debugging
    PPU& debug_get_ppu() { return mmu.get_ppu(); }
    // Allow mmu write access for unit tests
    void debug_write_io(uint16_t addr, uint8_t value) { mmu.write_byte(addr, value); }

private:
    void handle_input(const SDL_Event& event);

    MMU mmu;
    CPU cpu;

    SDL_Window* window;
    SDL_Renderer* renderer;
    SDL_Texture* texture;
    SDL_AudioStream* audio_stream;
    SDL_AudioDeviceID audio_device = 0;

    bool running;
    bool headless = false;
    int max_frames = 0; // 0 means run forever until window closed
};

#endif // EMULATOR_H