#include "ppu.h"
#include "mmu.h"
#include <iostream>
#include <algorithm>

PPU::PPU() : cycle_count(0) {
    // Initialize registers
    lcdc = 0x91;
    stat = 0x85;
    scy = scx = 0;
    ly = 0;
    lyc = 0;
    wy = wx = 0;
    bgp = 0xFC;
    obp0 = 0xFF;
    obp1 = 0xFF;

    framebuffer.fill(0xFFFFFFFF); // White background
}

PPU::~PPU() {
}

void PPU::step(uint8_t cycles, MMU& mmu) {
    cycle_count += cycles;

    // GameBoy PPU modes and timing
    // Mode 2: OAM Search (80 cycles)
    // Mode 3: Pixel Transfer (172 cycles)
    // Mode 0: HBlank (204 cycles)
    // Mode 1: VBlank (4560 cycles per frame)

    uint8_t current_mode = stat & 0x03;

    if (ly < 144) { // Visible lines
        if (cycle_count < 80) {
            // Mode 2: OAM Search
            if (current_mode != 2) {
                stat = (stat & ~0x03) | 0x02;
                // TODO: OAM interrupt if enabled
            }
        } else if (cycle_count < 80 + 172) {
            // Mode 3: Pixel Transfer
            if (current_mode != 3) {
                stat = (stat & ~0x03) | 0x03;
                // Render the current scanline at the start of pixel transfer
                render_scanline(mmu);
            }
        } else {
            // Mode 0: HBlank
            if (current_mode != 0) {
                stat = (stat & ~0x03) | 0x00;
                // TODO: HBlank interrupt if enabled
            }
        }
    } else {
        // Mode 1: VBlank
        if (current_mode != 1) {
            stat = (stat & ~0x03) | 0x01;
            // TODO: VBlank interrupt
        }
    }

    // Check for line completion
    if (cycle_count >= 456) {
        cycle_count -= 456;
        ly = (ly + 1) % 154;

        // Check LYC coincidence
        if (ly == lyc) {
            stat |= 0x04; // Set coincidence flag
            // TODO: Trigger STAT interrupt if enabled
        } else {
            stat &= ~0x04;
        }

        // VBlank start
        if (ly == 144) {
            // Trigger VBlank interrupt
            uint8_t if_reg = mmu.read_byte(0xFF0F);
            if_reg |= 0x01; // VBlank interrupt
            mmu.write_byte(0xFF0F, if_reg);
        }
    }
}

void PPU::render_scanline(MMU& mmu) {
    if (!(lcdc & 0x80)) return; // LCD disabled

    // Clear scanline
    for (int x = 0; x < 160; ++x) {
        framebuffer[ly * 160 + x] = 0xFF9BBC0F; // GameBoy green
    }

    if (lcdc & 0x01) render_background(mmu);
    if (lcdc & 0x20) render_window(mmu);
    if (lcdc & 0x02) render_sprites(mmu);
}

void PPU::render_background(MMU& mmu) {
    uint16_t bg_tile_map = (lcdc & 0x08) ? 0x9C00 : 0x9800;
    uint16_t tile_data = (lcdc & 0x10) ? 0x8000 : 0x8800;

    for (int x = 0; x < 160; ++x) {
        int bg_x = (x + scx) % 256;
        int bg_y = (ly + scy) % 256;

        int tile_x = bg_x / 8;
        int tile_y = bg_y / 8;

        uint16_t tile_map_addr = bg_tile_map + tile_y * 32 + tile_x;
        uint8_t tile_id = mmu.read_byte(tile_map_addr);

        if (!(lcdc & 0x10) && tile_id < 128) tile_id += 256; // Signed offset for 0x8800 mode

        uint16_t tile_addr = tile_data + tile_id * 16;
        uint8_t pixel = get_tile_pixel(mmu, tile_addr, bg_x % 8, bg_y % 8);

        uint32_t color = get_color(pixel, bgp);
        framebuffer[ly * 160 + x] = color;
    }
}

void PPU::render_window(MMU& mmu) {
    if (ly < wy) return;

    uint16_t win_tile_map = (lcdc & 0x40) ? 0x9C00 : 0x9800;
    uint16_t tile_data = (lcdc & 0x10) ? 0x8000 : 0x8800;

    int win_x = wx - 7;
    if (win_x >= 160) return;

    for (int x = std::max(0, win_x); x < 160; ++x) {
        int win_pixel_x = x - win_x;
        int win_pixel_y = ly - wy;

        int tile_x = win_pixel_x / 8;
        int tile_y = win_pixel_y / 8;

        uint16_t tile_map_addr = win_tile_map + tile_y * 32 + tile_x;
        uint8_t tile_id = mmu.read_byte(tile_map_addr);

        if (!(lcdc & 0x10) && tile_id < 128) tile_id += 256;

        uint16_t tile_addr = tile_data + tile_id * 16;
        uint8_t pixel = get_tile_pixel(mmu, tile_addr, win_pixel_x % 8, win_pixel_y % 8);

        uint32_t color = get_color(pixel, bgp);
        framebuffer[ly * 160 + x] = color;
    }
}

void PPU::render_sprites(MMU& mmu) {
    // Sprite rendering (OAM search and rendering)
    // GameBoy supports up to 40 sprites, but only 10 per scanline

    struct Sprite {
        uint8_t y, x, tile, attributes;
    };

    std::vector<Sprite> sprites_on_line;

    // Find sprites on current scanline
    for (int i = 0; i < 40; ++i) {
        uint16_t oam_addr = 0xFE00 + i * 4;
        Sprite sprite;
        sprite.y = mmu.read_byte(oam_addr);
        sprite.x = mmu.read_byte(oam_addr + 1);
        sprite.tile = mmu.read_byte(oam_addr + 2);
        sprite.attributes = mmu.read_byte(oam_addr + 3);

        // Check if sprite is on current scanline
        uint8_t sprite_height = (lcdc & 0x04) ? 16 : 8; // 8x16 or 8x8 mode
        if (ly + 16 >= sprite.y && ly + 16 < sprite.y + sprite_height) {
            sprites_on_line.push_back(sprite);
            if (sprites_on_line.size() >= 10) break; // Max 10 sprites per line
        }
    }

    // Sort sprites by X coordinate (lower X has higher priority)
    std::sort(sprites_on_line.begin(), sprites_on_line.end(),
              [](const Sprite& a, const Sprite& b) { return a.x < b.x; });

    // Render sprites (from lowest to highest priority)
    for (const auto& sprite : sprites_on_line) {
        uint8_t sprite_height = (lcdc & 0x04) ? 16 : 8;
        uint16_t tile_addr = 0x8000 + sprite.tile * 16;

        // Handle 8x16 sprites
        if (sprite_height == 16) {
            tile_addr = 0x8000 + (sprite.tile & 0xFE) * 16;
        }

        int sprite_y = ly - (sprite.y - 16);
        if (sprite.attributes & 0x40) { // Y flip
            sprite_y = sprite_height - 1 - sprite_y;
        }

        for (int x = 0; x < 8; ++x) {
            int screen_x = sprite.x + x - 8;
            if (screen_x < 0 || screen_x >= 160) continue;

            uint8_t pixel = get_tile_pixel(mmu, tile_addr, x, sprite_y);

            // Skip transparent pixels (color 0)
            if (pixel == 0) continue;

            // Check sprite priority
            bool behind_bg = sprite.attributes & 0x80;
            if (behind_bg) {
                // TODO: Check if background pixel is non-zero
                // For now, assume sprites are always in front
            }

            // Apply sprite palette
            uint8_t palette = (sprite.attributes & 0x10) ? obp1 : obp0;
            uint32_t color = get_color(pixel, palette);

            // Handle X flip
            int render_x = x;
            if (sprite.attributes & 0x20) { // X flip
                render_x = 7 - x;
            }

            framebuffer[ly * 160 + screen_x] = color;
        }
    }
}

uint8_t PPU::get_tile_pixel(MMU& mmu, uint16_t tile_addr, uint8_t x, uint8_t y) const {
    uint16_t row_addr = tile_addr + y * 2;
    uint8_t byte1 = mmu.read_byte(row_addr);
    uint8_t byte2 = mmu.read_byte(row_addr + 1);

    uint8_t bit = 7 - x;
    uint8_t pixel = ((byte1 & (1 << bit)) ? 1 : 0) | ((byte2 & (1 << bit)) ? 2 : 0);

    return pixel;
}

uint32_t PPU::get_color(uint8_t color_id, uint8_t palette) const {
    // GameBoy color palette (simplified)
    // 調亮 GameBoy 綠色調色盤
    static const uint32_t colors[4] = {
        0xFFFFFFFF, // White (最亮)
        0xFFB6FFB6, // Very light green
        0xFF7ED957, // Light green
        0xFF306230  // Dark green
    };

    uint8_t shade = (palette >> (color_id * 2)) & 0x03;
    return colors[shade];
}

// Register access functions
uint8_t PPU::get_lcdc() const {
    return lcdc;
}

void PPU::set_lcdc(uint8_t value) {
    lcdc = value;
}

uint8_t PPU::get_stat() const {
    return stat | 0x80; // Bit 7 always set
}

void PPU::set_stat(uint8_t value) {
    stat = (stat & 0x87) | (value & 0x78); // Preserve read-only bits
}