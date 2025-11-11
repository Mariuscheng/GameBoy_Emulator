#include "emulator.h"
#include <iostream>
#include <fstream>

Emulator::Emulator() : cpu(mmu), window(nullptr), renderer(nullptr), texture(nullptr), audio_stream(nullptr), running(false) {
}

Emulator::~Emulator() {
    shutdown();
}

bool Emulator::initialize() {
    // Initialize SDL
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO)) {
        std::cout << "SDL_Init Error: " << SDL_GetError() << std::endl;
        return false;
    }
    std::cout << "SDL initialized successfully" << std::endl;

    // Create window (GameBoy resolution: 160x144, scaled up)
    window = SDL_CreateWindow("GameBoy Emulator", 160 * 3, 144 * 3, SDL_WINDOW_RESIZABLE);
    if (!window) {
        std::cout << "SDL_CreateWindow Error: " << SDL_GetError() << std::endl;
        SDL_Quit();
        return false;
    }
    std::cout << "Window created successfully" << std::endl;

    // Create renderer
    renderer = SDL_CreateRenderer(window, nullptr);
    if (!renderer) {
        std::cout << "SDL_CreateRenderer Error: " << SDL_GetError() << std::endl;
        SDL_DestroyWindow(window);
        SDL_Quit();
        return false;
    }

    // Create texture for rendering
    texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_STREAMING, 160, 144);
    if (!texture) {
        std::cout << "SDL_CreateTexture Error: " << SDL_GetError() << std::endl;
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return false;
    }

    // Audio initialization (placeholder for now)
    audio_stream = nullptr;
    std::cout << "Audio not yet implemented (APU logic is ready)" << std::endl;

    // Initialize CPU and MMU
    cpu.reset();
    mmu.write_byte(0xFFFF, 0x1F); // Enable all interrupts (VBlank, LCD, Timer, Serial, Joypad)

    running = true;
    return true;
}

bool Emulator::load_rom(const std::string& rom_path) {
    std::ifstream file(rom_path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        std::cout << "Failed to open ROM file: " << rom_path << std::endl;
        return false;
    }

    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);

    std::vector<uint8_t> rom_data(size);
    if (!file.read(reinterpret_cast<char*>(rom_data.data()), size)) {
        std::cout << "Failed to read ROM file" << std::endl;
        return false;
    }

    if (!mmu.load_rom(rom_data)) {
        std::cout << "Failed to load ROM into memory" << std::endl;
        return false;
    }

    std::cout << "ROM loaded successfully: " << rom_path << " (" << size << " bytes)" << std::endl;
    return true;
}

void Emulator::run() {
    while (running) {
        if (window) {
            // Graphics mode
            SDL_Event event;
            while (SDL_PollEvent(&event)) {
                handle_input(event);
                if (event.type == SDL_EVENT_QUIT) {
                    running = false;
                }
            }

            // Update texture with PPU framebuffer
            const auto& framebuffer = mmu.get_ppu().get_framebuffer();
            SDL_UpdateTexture(texture, nullptr, framebuffer.data(), 160 * sizeof(uint32_t));

            // Clear screen
            SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
            SDL_RenderClear(renderer);

            // Render texture
            SDL_RenderTexture(renderer, texture, nullptr, nullptr);

            // Present
            SDL_RenderPresent(renderer);
        } else {
            // Headless mode - just run CPU
            SDL_Delay(16); // ~60 FPS
        }

        // Execute CPU instructions and update PPU and APU
        int total_cycles = 0;
        while (total_cycles < 70224) { // ~60 FPS worth of cycles (4.194304 MHz / 60)
            int cycles = cpu.step();
            total_cycles += cycles;
            mmu.get_ppu().step(cycles, mmu);
            mmu.get_apu().step(cycles);
        }

        // Exit after some time in headless mode
        if (!window) {
            static int counter = 0;
            if (++counter > 20000) { // Run for about 20000 frames (~5-6 minutes for test ROMs)
                running = false;
            }
        }
    }
}

void Emulator::shutdown() {
    if (texture) {
        SDL_DestroyTexture(texture);
        texture = nullptr;
    }
    if (renderer) {
        SDL_DestroyRenderer(renderer);
        renderer = nullptr;
    }
    if (window) {
        SDL_DestroyWindow(window);
        window = nullptr;
    }
    SDL_Quit();
}

void Emulator::handle_input(const SDL_Event& event) {
    if (event.type == SDL_EVENT_KEY_DOWN || event.type == SDL_EVENT_KEY_UP) {
        bool pressed = (event.type == SDL_EVENT_KEY_DOWN);
        int bit = -1;
        switch (event.key.key) {
            case SDLK_Z: bit = 0; break;  // A
            case SDLK_X: bit = 1; break;  // B
            case SDLK_RETURN: bit = 2; break;  // Start
            case SDLK_RSHIFT: bit = 3; break;  // Select
            case SDLK_DOWN: bit = 4; break;  // Down
            case SDLK_UP: bit = 5; break;  // Up
            case SDLK_LEFT: bit = 6; break;  // Left
            case SDLK_RIGHT: bit = 7; break;  // Right
        }
        if (bit != -1) {
            mmu.set_joypad_bit(bit, pressed);
            // Trigger Joypad interrupt
            uint8_t iflag = mmu.read_byte(0xFF0F);
            iflag |= 0x10;
            mmu.write_byte(0xFF0F, iflag);
        }
    }
}