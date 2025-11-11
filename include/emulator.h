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

private:
    void handle_input(const SDL_Event& event);

    MMU mmu;
    CPU cpu;

    SDL_Window* window;
    SDL_Renderer* renderer;
    SDL_Texture* texture;

    bool running;
};

#endif // EMULATOR_H