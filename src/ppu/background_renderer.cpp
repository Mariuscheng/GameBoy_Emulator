#include "background_renderer.h"
#include "mmu.h"
#include <iostream>
#include <algorithm>

// Static member initialization
int BackgroundRenderer::window_debug_lines_printed = 0;
int BackgroundRenderer::last_frame_ly = -1;
bool BackgroundRenderer::oam_dumped = false;

BackgroundRenderer::BackgroundRenderer() {
}

BackgroundRenderer::~BackgroundRenderer() {
}

void BackgroundRenderer::render_background(MMU& mmu, uint8_t ly, uint8_t lcdc, uint8_t scx, uint8_t scy, uint8_t bgp,
                                         std::array<uint32_t, 160 * 144>& framebuffer,
                                         std::array<uint8_t, 160 * 144>& bgwin_pixel_ids) {
    uint16_t bg_tile_map = (lcdc & 0x08) ? 0x9C00 : 0x9800;
    uint16_t tile_data = (lcdc & 0x10) ? 0x8000 : 0x8800;

    for (int x = 0; x < 160; ++x) {
        int bg_x = (x + scx) % 256;
        int bg_y = (ly + scy) % 256;

        int tile_x = bg_x / 8;
        int tile_y = bg_y / 8;

        uint16_t tile_map_addr = bg_tile_map + tile_y * 32 + tile_x;
        uint8_t tile_id = mmu.ppu_read(tile_map_addr);

        // Handle tile ID addressing based on LCDC.4
        uint16_t tile_addr;
        if (lcdc & 0x10) {
            // Unsigned addressing (0x8000 - 0x8FFF)
            // tile_id: 0-255, maps directly to tiles 0-255
            tile_addr = 0x8000 + (tile_id * 16);
        } else {
            // Signed addressing (0x8800 - 0x97FF)
            // tile_id: -128 to 127 (as signed int8)
            // Negative IDs (-128 to -1) map to tiles 128-255 in VRAM
            // Positive IDs (0 to 127) map to tiles 0-127 in VRAM
            int8_t signed_id = (int8_t)tile_id;
            tile_addr = 0x9000 + (signed_id * 16);
        }

        uint8_t pixel = get_tile_pixel(mmu, tile_addr, bg_x % 8, bg_y % 8);

        uint32_t color = get_color(pixel, bgp);
        framebuffer[ly * 160 + x] = color;
        bgwin_pixel_ids[ly * 160 + x] = pixel; // store raw color id for priority
    }
}

void BackgroundRenderer::render_window(MMU& mmu, uint8_t ly, uint8_t lcdc, uint8_t wy, uint8_t wx, uint8_t bgp,
                                      uint16_t win_line_counter,
                                      std::array<uint32_t, 160 * 144>& framebuffer,
                                      std::array<uint8_t, 160 * 144>& bgwin_pixel_ids) {
    // Window appears only when LY >= WY, WX >= 7, and WX <= 166 (hardware limit)
    if (ly < wy) return;
    if (wx < 7) return; // Hardware: WX < 7 不顯示 window
    if (wx > 166) return; // Outside drawable range; spec: only 0-166 inclusive shows

    uint16_t win_tile_map = (lcdc & 0x40) ? 0x9C00 : 0x9800;
    uint16_t tile_data = (lcdc & 0x10) ? 0x8000 : 0x8800;

    // Hardware subtracts 7 from WX to obtain left edge (WX=7 -> x=0)
    int win_x = (int)wx - 7;
    // If window starts off-screen to the left (WX < 7) we begin drawing at x=0
    if (win_x >= 160) return; // Starts beyond right edge

    for (int x = std::max(0, win_x); x < 160; ++x) {
        int win_pixel_x = x - win_x; // Window-local X (0..159)
        // Use internal window line counter for vertical addressing, not (LY - WY)
        int win_pixel_y = static_cast<int>(win_line_counter);

        // Clamp window coordinates to prevent out-of-bounds access
        if (win_pixel_x < 0 || win_pixel_x >= 160) continue;
        if (win_pixel_y < 0 || win_pixel_y >= 144) continue;

        int tile_x = win_pixel_x / 8;
        int tile_y = win_pixel_y / 8;

        // Safety check: ensure tile coordinates are within the 32x32 tile map
        if (tile_x < 0 || tile_x >= 32 || tile_y < 0 || tile_y >= 32) continue;

        uint16_t tile_map_addr = win_tile_map + tile_y * 32 + tile_x;
        uint8_t tile_id = mmu.ppu_read(tile_map_addr);

        // Handle tile ID addressing based on LCDC.4
        uint16_t tile_addr;
        if (lcdc & 0x10) {
            // Unsigned addressing (0x8000 - 0x8FFF)
            tile_addr = 0x8000 + (tile_id * 16);
        } else {
            // Signed addressing (0x8800 - 0x97FF)
            int8_t signed_id = (int8_t)tile_id;
            tile_addr = 0x9000 + (signed_id * 16);
        }

        uint8_t pixel = get_tile_pixel(mmu, tile_addr, win_pixel_x % 8, win_pixel_y % 8);
        // Window overwrites background unconditionally (even color 0)
        uint32_t color = get_color(pixel, bgp);
        framebuffer[ly * 160 + x] = color;
        bgwin_pixel_ids[ly * 160 + x] = pixel; // window overwrites background id
    }
}

uint8_t BackgroundRenderer::get_tile_pixel(MMU& mmu, uint16_t tile_addr, uint8_t x, uint8_t y) const {
    uint16_t row_addr = tile_addr + y * 2;
    uint8_t byte1 = mmu.ppu_read(row_addr);
    uint8_t byte2 = mmu.ppu_read(row_addr + 1);

    // Extract the bit for this pixel (bit 7-x)
    uint8_t bit1 = (byte1 >> (7 - x)) & 0x01;
    uint8_t bit2 = (byte2 >> (7 - x)) & 0x01;

    // Combine bits to get color ID (0-3)
    return (bit2 << 1) | bit1;
}

uint32_t BackgroundRenderer::get_color(uint8_t color_id, uint8_t palette) const {
    // Extract the color from palette based on color_id
    uint8_t shift = color_id * 2;
    uint8_t color_value = (palette >> shift) & 0x03;

    // Game Boy color palette (simplified grayscale for background)
    switch (color_value) {
        case 0: return 0xFFFFFFFF; // White
        case 1: return 0xFFAAAAAA; // Light gray
        case 2: return 0xFF555555; // Dark gray
        case 3: return 0xFF000000; // Black
        default: return 0xFFFFFFFF;
    }
}