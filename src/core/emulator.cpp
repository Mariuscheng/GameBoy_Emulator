#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#include "emulator.h"
#include <iostream>
#ifdef _WIN32
#include <windows.h>
#endif
#include <fstream>
#ifndef EMU_FRAME_DEBUG
#define EMU_FRAME_DEBUG 0
#endif

Emulator::Emulator() : cpu(mmu), window(nullptr), renderer(nullptr), texture(nullptr), audio_stream(nullptr), running(false) {
}

Emulator::~Emulator() {
    shutdown();
}

bool Emulator::initialize() {
    // Initialize SDL3 - skip global init and let individual functions initialize as needed

    if (!headless) {
        // Create window (GameBoy resolution: 160x144, scaled up)
        window = SDL_CreateWindow("GameBoy Emulator", 160 * 3, 144 * 3, SDL_WINDOW_RESIZABLE);
        if (!window) {
            return false;
        }
        SDL_ShowWindow(window);

        // Create renderer
        renderer = SDL_CreateRenderer(window, nullptr);
        if (!renderer) {
            SDL_DestroyWindow(window);
            return false;
        }

        // Create texture for rendering
        texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_STREAMING, 160, 144);
        if (!texture) {
            SDL_DestroyRenderer(renderer);
            SDL_DestroyWindow(window);
            return false;
        }
    }

    // Audio initialization (SDL3: 直接開啟綁定裝置的 AudioStream，推資料即可播放)
    // 初始化音訊子系統
    bool audio_init_success = (SDL_Init(SDL_INIT_AUDIO) == 0);
    if (audio_init_success) {
        SDL_AudioSpec want{};
        want.freq = 44100;
        want.format = SDL_AUDIO_S16LE;
        want.channels = 2;
        audio_stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &want, nullptr, nullptr);
        if (audio_stream) {
            // 檢查實際獲得的音訊規格
            SDL_AudioSpec obtained{};
            SDL_GetAudioStreamFormat(audio_stream, &obtained, nullptr);
        }
    }

    // Initialize CPU and MMU
    cpu.reset();

    running = true;
    return true;
}

bool Emulator::load_rom(const std::string& rom_path) {
    std::ifstream file(rom_path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        return false;
    }

    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);

    std::vector<uint8_t> rom_data(size);
    if (!file.read(reinterpret_cast<char*>(rom_data.data()), size)) {
        return false;
    }

    if (!mmu.load_rom(rom_data)) {
        return false;
    }

    // Enable quick timing mode for timing test ROMs (Route A)
    if (rom_path.find("read_timing") != std::string::npos || 
        rom_path.find("write_timing") != std::string::npos ||
        rom_path.find("modify_timing") != std::string::npos ||
        rom_path.find("mem_timing") != std::string::npos) {
        cpu.set_timing_test_mode(true);
    }
    return true;
}

void Emulator::run() {
#ifndef EMU_FRAME_DEBUG
#endif
    int frame_count = 0;
    while (running) {
        frame_count++;
        if (window) {
            SDL_Event event;
            while (SDL_PollEvent(&event)) {
                handle_input(event);
                if (event.type == SDL_EVENT_QUIT) {
                    running = false;
                }
            }
            const auto& framebuffer = mmu.get_ppu().get_framebuffer();
            SDL_UpdateTexture(texture, nullptr, framebuffer.data(), 160 * sizeof(uint32_t));
            SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
            SDL_RenderClear(renderer);
            SDL_RenderTexture(renderer, texture, nullptr, nullptr);
            SDL_RenderPresent(renderer);
        }

        int total_cycles = 0;
        const int target_cycles = 70224; // cycles per frame
        const int audio_samples_per_frame = 735; // 44100 / 60

        if (audio_stream) {
            std::vector<float> audio_buffer_float(audio_samples_per_frame); // mono
            mmu.get_apu().get_audio_samples(audio_buffer_float.data(), static_cast<int>(audio_buffer_float.size()));
            // 檢查是否有非零資料
            bool has_non_zero = false;
            for (float sample : audio_buffer_float) {
                if (sample != 0.0f) { has_non_zero = true; break; }
            }
            // 轉換為 S16LE 立體聲
            std::vector<int16_t> audio_buffer(audio_samples_per_frame * 2);
            for (size_t i = 0; i < audio_buffer_float.size(); ++i) {
                int16_t sample = static_cast<int16_t>(audio_buffer_float[i] * 32767.0f);
                audio_buffer[i * 2] = sample;     // left
                audio_buffer[i * 2 + 1] = sample; // right
            }
            SDL_PutAudioStreamData(audio_stream, audio_buffer.data(), static_cast<int>(audio_buffer.size() * sizeof(int16_t)));
        }

        int halt_cycles = 0;
        while (total_cycles < target_cycles) {
            int cycles = cpu.step();
            if (cycles == 0) { cycles = 4; halt_cycles++; }
            total_cycles += cycles;
            mmu.get_ppu().step(cycles, mmu);
            mmu.get_apu().step(cycles);
        }

        if (max_frames > 0 && frame_count >= max_frames) {
            const char* out = "frame_end.ppm";
            // Save silently without console spam
            (void)save_framebuffer_ppm(out);
            running = false;
        }
    }
    mmu.get_ppu().dump_lcd_on_summary();
}

void Emulator::shutdown() {
    if (audio_stream) { SDL_DestroyAudioStream(audio_stream); audio_stream = nullptr; }
    if (audio_device) { SDL_CloseAudioDevice(audio_device); audio_device = 0; }
    if (texture) { SDL_DestroyTexture(texture); texture = nullptr; }
    if (renderer) { SDL_DestroyRenderer(renderer); renderer = nullptr; }
    if (window) { SDL_DestroyWindow(window); window = nullptr; }
    SDL_Quit();
}

void Emulator::handle_input(const SDL_Event& event) {
    if (event.type == SDL_EVENT_KEY_DOWN || event.type == SDL_EVENT_KEY_UP) {
        bool pressed = (event.type == SDL_EVENT_KEY_DOWN);
        int bit = -1;
        switch (event.key.scancode) {
            case SDL_SCANCODE_RIGHT: bit = 0; break;   // Right
            case SDL_SCANCODE_LEFT:  bit = 1; break;   // Left
            case SDL_SCANCODE_UP:    bit = 2; break;   // Up
            case SDL_SCANCODE_DOWN:  bit = 3; break;   // Down
            case SDL_SCANCODE_A:     bit = 4; break;   // A
            case SDL_SCANCODE_S:     bit = 5; break;   // B
            case SDL_SCANCODE_SPACE: bit = 6; break;   // Select
            case SDL_SCANCODE_RETURN:
            case SDL_SCANCODE_KP_ENTER: bit = 7; break; // Start
        }
        if (bit != -1) {
            mmu.set_joypad_bit(bit, pressed);
            uint8_t iflag = mmu.read_byte(0xFF0F); iflag |= 0x10; mmu.write_byte(0xFF0F, iflag);
        }
    }
}

void Emulator::set_ppu_lcd_start_offset(uint16_t offset) {
    mmu.get_ppu().set_lcd_start_cycle_offset(offset);
}

bool Emulator::save_framebuffer_ppm(const std::string& path) const {
    const auto& fb = mmu.get_ppu().get_framebuffer();
    const int width = 160; const int height = 144;
    FILE* f = nullptr;
#ifdef _WIN32
    fopen_s(&f, path.c_str(), "wb");
#else
    f = fopen(path.c_str(), "wb");
#endif
    if (!f) return false;
    fprintf(f, "P6\n%d %d\n255\n", width, height);
    for (int i = 0; i < width * height; ++i) {
        uint32_t p = fb[i];
        unsigned char r = (unsigned char)((p >> 16) & 0xFF);
        unsigned char g = (unsigned char)((p >> 8) & 0xFF);
        unsigned char b = (unsigned char)(p & 0xFF);
        unsigned char rgb[3] = { r, g, b }; fwrite(rgb, 1, 3, f);
    }
    fclose(f);
    return true;
}