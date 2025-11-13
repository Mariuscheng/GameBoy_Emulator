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
    std::cout << "Window created successfully - showing window..." << std::endl;
    SDL_ShowWindow(window);
    std::cout << "Window shown" << std::endl;

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

    // Audio initialization
    SDL_AudioSpec desired_spec;
    SDL_zero(desired_spec);
    desired_spec.freq = 44100;
    desired_spec.format = SDL_AUDIO_F32;
    desired_spec.channels = 2;

    audio_stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &desired_spec, nullptr, nullptr);
    if (!audio_stream) {
        std::cout << "SDL_OpenAudioDeviceStream Error: " << SDL_GetError() << std::endl;
        SDL_DestroyTexture(texture);
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return false;
    }

    // Get the audio device from the stream and resume it
    SDL_AudioDeviceID device = SDL_GetAudioStreamDevice(audio_stream);
    SDL_ResumeAudioDevice(device);
    std::cout << "Audio initialized successfully" << std::endl;

    // Initialize CPU and MMU
    cpu.reset();
    // Don't enable interrupts here - let the ROM handle initialization
    // mmu.write_byte(0xFFFF, 0x1F); // Enable all interrupts (VBlank, LCD, Timer, Serial, Joypad)

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
    std::cout << "=== EMULATOR RUN STARTED ===" << std::endl;
    std::cout.flush();
    std::cout << "Starting emulator main loop..." << std::endl;
    std::cout.flush();
    std::cout << "Window pointer: " << window << std::endl;
    std::cout.flush();
    std::cout << "Running flag: " << running << std::endl;
    std::cout.flush();
    int frame_count = 0;
    
    std::cout << "About to enter main loop, running=" << running << std::endl;
    std::cout.flush();
    while (running) {
        frame_count++;
        std::cout << ">>> FRAME " << frame_count << " START <<<" << std::endl;
        std::cout.flush();
        
        if (window) {
            // Graphics mode
            std::cout << "FRAME " << frame_count << ": Graphics mode - window exists" << std::endl;
            std::cout.flush();
            SDL_Event event;
            while (SDL_PollEvent(&event)) {
                handle_input(event);
                if (event.type == SDL_EVENT_QUIT) {
                    running = false;
                }
            }

            // Update texture with PPU framebuffer
            const auto& framebuffer = mmu.get_ppu().get_framebuffer();
            std::cout << "FRAME " << frame_count << ": Got framebuffer, size=" << framebuffer.size() << std::endl;
            std::cout.flush();
            SDL_UpdateTexture(texture, nullptr, framebuffer.data(), 160 * sizeof(uint32_t));

            // Clear screen
            SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
            SDL_RenderClear(renderer);

            // Render texture
            SDL_RenderTexture(renderer, texture, nullptr, nullptr);

            // Present
            SDL_RenderPresent(renderer);
            std::cout << "FRAME " << frame_count << ": Graphics rendered and presented" << std::endl;
            std::cout.flush();
        } else {
            // Headless mode - just run CPU
            std::cout << "FRAME " << frame_count << ": Headless mode - no window" << std::endl;
            std::cout.flush();
            SDL_Delay(16); // ~60 FPS
        }

        // Execute CPU instructions and update PPU and APU
        int total_cycles = 0;
        const int target_cycles = 70224; // ~60 FPS worth of cycles (4.194304 MHz / 60)
        const int audio_samples_per_frame = 735; // 44100 Hz / 60 FPS ≈ 735 samples per frame

        // Generate audio samples for this frame
        if (audio_stream) {
            std::vector<float> audio_buffer(audio_samples_per_frame * 2); // Stereo
            mmu.get_apu().get_audio_samples(audio_buffer.data(), audio_buffer.size());
            SDL_PutAudioStreamData(audio_stream, audio_buffer.data(), audio_buffer.size() * sizeof(float));
        }

        std::cout << "FRAME " << frame_count << ": About to execute CPU cycles" << std::endl;
        std::cout.flush();
        int halt_cycles = 0;
        int loop_count = 0;
        while (total_cycles < target_cycles) {
            loop_count++;
            int cycles = cpu.step();
            // If CPU is halted (returns 0), still need to advance PPU/APU
            if (cycles == 0) {
                cycles = 4; // 1 M-cycle minimum
                halt_cycles++;
                if (halt_cycles <= 50) {
                    std::cout << "[Emulator] CPU halted at total=" << total_cycles << ", forcing 4 cycles" << std::endl;
                }
            }
            if (loop_count <= 100 || loop_count % 10000 == 0) {
                std::cout << "[Emulator loop " << loop_count << "] CPU returned " << cycles << " cycles, total=" << total_cycles << std::endl;
            }
            total_cycles += cycles;
            mmu.get_ppu().step(cycles, mmu);
            mmu.get_apu().step(cycles);
        }
        std::cout << "FRAME " << frame_count << ": CPU execution complete, total_cycles=" << total_cycles << ", loop_count=" << loop_count << std::endl;
        std::cout.flush();
        std::cout << ">>> FRAME " << frame_count << " END <<<" << std::endl;
        std::cout.flush();
    }
    std::cout << "=== EMULATOR RUN ENDED ===" << std::endl;
    std::cout.flush();
}

void Emulator::shutdown() {
    if (audio_stream) {
        SDL_DestroyAudioStream(audio_stream);
        audio_stream = nullptr;
    }
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