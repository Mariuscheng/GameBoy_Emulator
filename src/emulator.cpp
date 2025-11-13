#include "emulator.h"
#include <iostream>
#ifdef _WIN32
#include <windows.h>
#endif
#include <fstream>
#ifndef EMU_FRAME_DEBUG
#define EMU_FRAME_DEBUG 0
#endif

Emulator::Emulator() : cpu(mmu), window(nullptr), renderer(nullptr), texture(nullptr), audio_stream(nullptr), running(false), headless(false) {
}

Emulator::~Emulator() {
    shutdown();
}

bool Emulator::initialize() {
    // Initialize SDL (SDL_Init returns 0 on success, negative on failure)
    // 僅允許SDL初始化成功才繼續，否則直接失敗
    // 啟動前自動檢查並嘗試複製SDL3.dll等必要檔案
#ifdef _WIN32
    const char* dlls[] = {"SDL3.dll", "SDL3_image.dll", "SDL3_ttf.dll"};
    const char* searchPaths[] = {
        "C:/vcpkg/installed/x64-windows/bin/",
        "C:/SDL3/bin/",
        "C:/msys64/mingw64/bin/"
    };
    char exePath[MAX_PATH];
    GetModuleFileNameA(NULL, exePath, MAX_PATH);
    std::string exeDir = std::string(exePath);
    exeDir = exeDir.substr(0, exeDir.find_last_of("/\\")) + "/";
    for (const char* dll : dlls) {
        std::string target = exeDir + dll;
        std::ifstream f(target);
        if (!f.good()) {
            bool copied = false;
            for (const char* srcDir : searchPaths) {
                std::string src = std::string(srcDir) + dll;
                std::ifstream fs(src, std::ios::binary);
                if (fs.good()) {
                    std::ofstream ft(target, std::ios::binary);
                    ft << fs.rdbuf();
                    std::cout << "[AutoFix] Copied " << dll << " from " << srcDir << std::endl;
                    copied = true;
                    break;
                }
            }
            if (!copied) {
                std::cout << "[AutoFix] Missing " << dll << ", please copy it to " << exeDir << std::endl;
            }
        }
    }
#endif
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) == 0) {
        std::cout << "[Init] SDL initialized successfully (video+audio)" << std::endl;
    } else {
        std::string sdl_err = SDL_GetError();
        std::cout << "[Init] SDL_Init VIDEO|AUDIO failed: " << sdl_err << std::endl;
        std::cout << "[ERROR] SDL could not be initialized. Please ensure SDL3.dll is present in the executable directory." << std::endl;
#ifdef _WIN32
        std::cout << "[HINT] On Windows, place SDL3.dll, SDL3_image.dll, SDL3_ttf.dll in the same folder as GameBoy.exe." << std::endl;
#endif
        return false;
    }

    if (!headless) {
        // Create window (GameBoy resolution: 160x144, scaled up)
        window = SDL_CreateWindow("GameBoy Emulator", 160 * 3, 144 * 3, SDL_WINDOW_RESIZABLE);
        if (!window) {
            std::cout << "[Init] SDL_CreateWindow failed: " << SDL_GetError() << std::endl;
            SDL_Quit();
            return false;
        }
        std::cout << "[Init] Window created successfully - showing window..." << std::endl;
        SDL_ShowWindow(window);
        std::cout << "[Init] Window shown" << std::endl;

        // Create renderer
        renderer = SDL_CreateRenderer(window, nullptr);
        if (!renderer) {
            std::cout << "[Init] SDL_CreateRenderer failed: " << SDL_GetError() << std::endl;
            SDL_DestroyWindow(window);
            SDL_Quit();
            return false;
        }

        // Create texture for rendering
        texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_STREAMING, 160, 144);
        if (!texture) {
            std::cout << "[Init] SDL_CreateTexture failed: " << SDL_GetError() << std::endl;
            SDL_DestroyRenderer(renderer);
            SDL_DestroyWindow(window);
            SDL_Quit();
            return false;
        }
    }

    // Audio initialization (disabled for now)
    audio_stream = nullptr;

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
    // Basic start banner (can disable with EMU_FRAME_DEBUG if desired)
#ifndef EMU_FRAME_DEBUG
    // Minimal banner only once
    std::cout << "Emulator starting..." << std::endl;
#else
    std::cout << "=== EMULATOR RUN STARTED ===" << std::endl;
    std::cout << "Starting emulator main loop..." << std::endl;
    std::cout << "Window pointer: " << window << std::endl;
    std::cout << "Running flag: " << running << std::endl;
#endif
    int frame_count = 0;
    
    std::cout << "About to enter main loop, running=" << running << std::endl;
    std::cout.flush();
    while (running) {
        frame_count++;
    #if EMU_FRAME_DEBUG
        std::cout << ">>> FRAME " << frame_count << " START <<<" << std::endl;
    #endif
        
        if (!headless) {
    #if EMU_FRAME_DEBUG
            std::cout << "FRAME " << frame_count << ": Graphics mode - window exists" << std::endl;
    #endif
            SDL_Event event;
            while (SDL_PollEvent(&event)) {
                handle_input(event);
                if (event.type == SDL_EVENT_QUIT) {
                    running = false;
                }
            }

            // Update texture with PPU framebuffer
            const auto& framebuffer = mmu.get_ppu().get_framebuffer();
            
#if EMU_FRAME_DEBUG
            std::cout << "FRAME " << frame_count << ": Got framebuffer, size=" << framebuffer.size() << std::endl;
#endif
            SDL_UpdateTexture(texture, nullptr, framebuffer.data(), 160 * sizeof(uint32_t));

            // Clear screen
            SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
            SDL_RenderClear(renderer);

            // Render texture
            SDL_RenderTexture(renderer, texture, nullptr, nullptr);

            // Present
            SDL_RenderPresent(renderer);
            
#if EMU_FRAME_DEBUG
            std::cout << "FRAME " << frame_count << ": Graphics rendered and presented" << std::endl;
#endif
        } else {
            // Headless mode - just run CPU
            
#if EMU_FRAME_DEBUG
            std::cout << "FRAME " << frame_count << ": Headless mode - no window" << std::endl;
#endif
            // No delay in headless mode to run as fast as possible
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

        
    #if EMU_FRAME_DEBUG
        std::cout << "FRAME " << frame_count << ": About to execute CPU cycles" << std::endl;
    #endif
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
            // Verbose per-instruction loop debug removed (was printing first 100 and every 10000 loops)
            // If needed for future diagnostics, wrap similar code with a macro EMU_LOOP_DEBUG.
            total_cycles += cycles;
            mmu.get_ppu().step(cycles, mmu);
            mmu.get_apu().step(cycles);
        }
        
    #if EMU_FRAME_DEBUG
        std::cout << "FRAME " << frame_count << ": CPU execution complete, total_cycles=" << total_cycles << ", loop_count=" << loop_count << std::endl;
    #endif
        if (frame_count % 60 == 0) { // Every ~1 second of emulated time
    #if EMU_FRAME_DEBUG
            std::cout << "[CPU HALT STATS] frame=" << frame_count
                  << " halt_count=" << cpu.halt_count
                  << " halt_bug_count=" << cpu.halt_bug_count << std::endl;
    #endif
        }
    #if EMU_FRAME_DEBUG
        std::cout << ">>> FRAME " << frame_count << " END <<<" << std::endl;
    #endif

        // Optional frame limit to assist automated testing
        if (max_frames > 0 && frame_count >= max_frames) {
            // Try saving a screenshot before exiting
            const char* out = "frame_end.ppm";
            if (save_framebuffer_ppm(out)) {
                std::cout << "[SCREENSHOT] Saved framebuffer to " << out << std::endl;
            } else {
                std::cout << "[SCREENSHOT] Failed to save framebuffer" << std::endl;
            }
            running = false;
        }
    }
    
    #if EMU_FRAME_DEBUG
        std::cout << "=== EMULATOR RUN ENDED ===" << std::endl;
    #endif
    // 執行 LCDC ON 事件摘要輸出 (#3)
    mmu.get_ppu().dump_lcd_on_summary();
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
    if (!headless) {
        SDL_Quit();
    }
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

void Emulator::set_ppu_lcd_start_offset(uint16_t offset) {
    mmu.get_ppu().set_lcd_start_cycle_offset(offset);
    std::cout << "[Emulator] Set PPU lcd_start_cycle_offset=" << offset << std::endl;
}

bool Emulator::save_framebuffer_ppm(const std::string& path) const {
    // Dump current PPU framebuffer (RGBA32) to a binary PPM (P6, RGB)
    const auto& fb = mmu.get_ppu().get_framebuffer();
    const int width = 160;
    const int height = 144;
    FILE* f = nullptr;
#ifdef _WIN32
    fopen_s(&f, path.c_str(), "wb");
#else
    f = fopen(path.c_str(), "wb");
#endif
    if (!f) return false;
    // Header
    fprintf(f, "P6\n%d %d\n255\n", width, height);
    // Pixels
    for (int i = 0; i < width * height; ++i) {
        uint32_t p = fb[i];
        unsigned char r = (unsigned char)((p >> 16) & 0xFF);
        unsigned char g = (unsigned char)((p >> 8) & 0xFF);
        unsigned char b = (unsigned char)(p & 0xFF);
        unsigned char rgb[3] = { r, g, b };
        fwrite(rgb, 1, 3, f);
    }
    fclose(f);
    return true;
}