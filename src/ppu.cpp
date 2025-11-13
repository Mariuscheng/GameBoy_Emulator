#include "ppu.h"
#include "mmu.h"
#include <iostream>
#include <algorithm>

PPU::PPU() : cycle_count(0), shadow_scx(0), shadow_scy(0), ppu_mode(2) {
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

    framebuffer.fill(0xFFFFFFFF); // White background (will be overwritten per scanline)
    bgwin_pixel_ids.fill(0);      // All background pixels start as color 0
}

PPU::~PPU() {
}

void PPU::step(int cycles, MMU& mmu) {
    for (int i = 0; i < cycles; ++i) {
        // Determine mode based on whether we're in VBlank or visible area
        uint8_t new_mode;
        
        if (ly >= 144) {
            // VBlank period (LY 144-153): always Mode 1
            new_mode = 1;
        } else {
            // Visible scanlines (LY 0-143): modes 0, 2, 3 based on cycle_count
            if (cycle_count < 80) {
                new_mode = 2; // OAM Search
            } else if (cycle_count < 252) {
                new_mode = 3; // Pixel Transfer
            } else {
                new_mode = 0; // HBlank
            }
        }

        // Handle mode changes
        if (new_mode != ppu_mode) {
            ppu_mode = new_mode;
            stat = (stat & ~0x03) | ppu_mode;

            if (ppu_mode == 2) {
                // Mode 2 (OAM Search)
                if (stat & 0x20) {
                    uint8_t if_reg = mmu.read_byte(0xFF0F);
                    if_reg |= 0x02;
                    mmu.write_byte(0xFF0F, if_reg);
                }
            } else if (ppu_mode == 3) {
                // Mode 3 (Pixel Transfer) - snapshot scroll values and render
                shadow_scx = scx;
                shadow_scy = scy;
                render_scanline(mmu, shadow_scx, shadow_scy);
            } else if (ppu_mode == 0) {
                // Mode 0 (HBlank)
                if (stat & 0x08) {
                    uint8_t if_reg = mmu.read_byte(0xFF0F);
                    if_reg |= 0x02;
                    mmu.write_byte(0xFF0F, if_reg);
                }
            } else if (ppu_mode == 1) {
                // Mode 1 (VBlank) - triggered at LY=144
                if (ly == 144) {
                    uint8_t if_reg = mmu.read_byte(0xFF0F);
                    if_reg |= 0x01; // VBlank interrupt
                    mmu.write_byte(0xFF0F, if_reg);

                    // STAT mode 1 interrupt
                    if (stat & 0x10) {
                        if_reg = mmu.read_byte(0xFF0F);
                        if_reg |= 0x02;
                        mmu.write_byte(0xFF0F, if_reg);
                    }
                }
            }
        }

        cycle_count++;

        // One-time debug of initial register setup for acid2 layout issues
        if (!frame_info_printed && ly == 0 && cycle_count == 1) {
            std::cout << "[PPU] Frame start LCDC=" << std::hex << (int)lcdc
                      << " SCX=" << (int)scx << " SCY=" << (int)scy
                      << " WX=" << (int)wx << " WY=" << (int)wy << std::dec << std::endl;
            frame_info_printed = true;
        }

        // End of scanline
        if (cycle_count == 456) {
            cycle_count = 0;

            uint8_t old_ly = ly;
            ly++;

            // LYC coincidence
            if (ly == lyc) {
                stat |= 0x04;
                if (stat & 0x40) {
                    uint8_t if_reg = mmu.read_byte(0xFF0F);
                    if_reg |= 0x02;
                    mmu.write_byte(0xFF0F, if_reg);
                }
            } else {
                stat &= ~0x04;
            }

            // Frame reset when LY reaches 154
            if (ly == 154) {
                ly = 0;
                ppu_mode = 2;
                stat = (stat & ~0x03) | 0x02;

                // STAT mode 2 interrupt
                if (stat & 0x20) {
                    uint8_t if_reg = mmu.read_byte(0xFF0F);
                    if_reg |= 0x02;
                    mmu.write_byte(0xFF0F, if_reg);
                }
            }
        }
    }
}

void PPU::render_scanline(MMU& mmu, uint8_t shadow_scx, uint8_t shadow_scy) {
    if (!(lcdc & 0x80)) return; // LCD disabled
    if (ly >= 144) return; // Don't render during VBlank (LY 144-153)

    // Clear scanline with background color ID 0 (palette mapped)
    uint32_t base_color = get_color(0, bgp);
    for (int x = 0; x < 160; ++x) {
        framebuffer[ly * 160 + x] = base_color;
        bgwin_pixel_ids[ly * 160 + x] = 0; // raw color id 0
    }

    if (lcdc & 0x01) render_background(mmu, shadow_scx, shadow_scy);
    if (lcdc & 0x20) render_window(mmu, shadow_scx, shadow_scy);
    if (lcdc & 0x02) render_sprites(mmu);
}

void PPU::render_background(MMU& mmu, uint8_t shadow_scx, uint8_t shadow_scy) {
    uint16_t bg_tile_map = (lcdc & 0x08) ? 0x9C00 : 0x9800;
    uint16_t tile_data = (lcdc & 0x10) ? 0x8000 : 0x8800;

    for (int x = 0; x < 160; ++x) {
        int bg_x = (x + shadow_scx) % 256;
        int bg_y = (ly + shadow_scy) % 256;

        int tile_x = bg_x / 8;
        int tile_y = bg_y / 8;

        uint16_t tile_map_addr = bg_tile_map + tile_y * 32 + tile_x;
        uint8_t tile_id = mmu.read_byte(tile_map_addr);

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

void PPU::render_window(MMU& mmu, uint8_t shadow_scx, uint8_t shadow_scy) {
    // Window appears only when LY >= WY and WX <= 166 (hardware limit)
    if (ly < wy) return;
    if (wx > 166) return; // Outside drawable range; spec: only 0-166 inclusive shows

    uint16_t win_tile_map = (lcdc & 0x40) ? 0x9C00 : 0x9800;
    uint16_t tile_data = (lcdc & 0x10) ? 0x8000 : 0x8800;

    // Hardware subtracts 7 from WX to obtain left edge (WX=7 -> x=0)
    int win_x = (int)wx - 7;
    // If window starts off-screen to the left (WX < 7) we begin drawing at x=0
    if (win_x >= 160) return; // Starts beyond right edge

    for (int x = std::max(0, win_x); x < 160; ++x) {
        int win_pixel_x = x - win_x; // Window-local X (0..159)
        int win_pixel_y = ly - wy;

        // Clamp window coordinates to prevent out-of-bounds access
        if (win_pixel_x < 0 || win_pixel_x >= 160) continue;
        if (win_pixel_y < 0 || win_pixel_y >= 144) continue;

        int tile_x = win_pixel_x / 8;
        int tile_y = win_pixel_y / 8;

        // Safety check: ensure tile coordinates are within the 32x32 tile map
        if (tile_x < 0 || tile_x >= 32 || tile_y < 0 || tile_y >= 32) continue;

        uint16_t tile_map_addr = win_tile_map + tile_y * 32 + tile_x;
        uint8_t tile_id = mmu.read_byte(tile_map_addr);

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

    // 擴充除錯：前 12 個 window 行 (從 ly==wy 起) 列印右側 6 個 tile id 與 WY/WX
    // 每一幀重新列印前 12 window 行的右側 tile。當 ly==0 (新幀開始) 時重置計數。
    static int window_debug_lines_printed = 0;
    static int last_frame_ly = -1;
    if (ly == 0 && last_frame_ly != 0) { // 新幀開始（LY 回到 0）
        window_debug_lines_printed = 0;
    }
    last_frame_ly = ly;

    if (ly >= wy && window_debug_lines_printed < 12) {
        int win_line = ly - wy; // window 內部行
        // 右側顯示範圍：tile_x 14..19 (總寬 20 tiles)，集中觀察右下異常區
        std::cout << "[PPU] WIN line=" << win_line << " LY=" << (int)ly
                  << " WY=" << (int)wy << " WX=" << (int)wx
                  << " win_x=" << (int)win_x << " tile_y=" << (win_line/8) << " : ";
        for (int tx = 14; tx < 20; ++tx) {
            int ty = win_line / 8;
            uint16_t addr = win_tile_map + ty * 32 + tx;
            std::cout << std::hex << (int)mmu.read_byte(addr) << ' ';
        }
        std::cout << std::dec << std::endl;
    window_debug_lines_printed++;

        // 一次性 OAM dump：第一個 window 行時列出所有 sprite 參數以檢查是否出現預期圖樣
        static bool oam_dumped = false;
        if (!oam_dumped && win_line == 0) {
            std::cout << "[PPU] OAM dump (index:y x tile attr)" << std::endl;
            for (int i = 0; i < 40; ++i) {
                uint16_t oam_addr = 0xFE00 + i * 4;
                uint8_t sy = mmu.read_byte(oam_addr);
                uint8_t sx = mmu.read_byte(oam_addr + 1);
                uint8_t st = mmu.read_byte(oam_addr + 2);
                uint8_t sa = mmu.read_byte(oam_addr + 3);
                // 僅列出可能在右側顯示區的 sprite 以減少噪音
                if (sx >= 80) {
                    std::cout << "  [" << i << "] " << (int)sy << " " << (int)sx << " " << std::hex << (int)st << " " << (int)sa << std::dec << std::endl;
                }
            }
            oam_dumped = true;
        }
    }
}

void PPU::render_sprites(MMU& mmu) {
    // Sprite rendering (OAM search and rendering)
    // GameBoy supports up to 40 sprites, but only 10 per scanline

    struct Sprite {
        uint8_t y, x, tile, attributes;
    };

    std::vector<Sprite> sprites_on_line;
    // OAM order scanning: take first 10 sprites appearing on this line
    uint8_t sprite_height_global = (lcdc & 0x04) ? 16 : 8;
    for (int i = 0; i < 40 && sprites_on_line.size() < 10; ++i) {
        uint16_t oam_addr = 0xFE00 + i * 4;
        Sprite sprite{ mmu.read_byte(oam_addr), mmu.read_byte(oam_addr + 1),
                       mmu.read_byte(oam_addr + 2), mmu.read_byte(oam_addr + 3) };
        if (sprite.y == 0 || sprite.x == 0) continue; // Hidden sprites (hardware treats 0 as off-screen)
        if (ly + 16 >= sprite.y && ly + 16 < sprite.y + sprite_height_global) {
            sprites_on_line.push_back(sprite);
        }
    }

    // Render sprites in OAM order (earlier index has priority)
    for (const auto& sprite : sprites_on_line) {
        uint8_t sprite_height = (lcdc & 0x04) ? 16 : 8;
        int line_in_sprite = ly - (sprite.y - 16); // line relative to top of sprite
        if (line_in_sprite < 0 || line_in_sprite >= sprite_height) continue;

        bool yflip = (sprite.attributes & 0x40) != 0;
        bool xflip = (sprite.attributes & 0x20) != 0;
        bool behind_bg = (sprite.attributes & 0x80) != 0;
        uint8_t palette = (sprite.attributes & 0x10) ? obp1 : obp0;

        int effective_line = yflip ? (sprite_height - 1 - line_in_sprite) : line_in_sprite;
        uint8_t base_tile_index = (sprite_height == 16) ? (sprite.tile & 0xFE) : sprite.tile;
        uint8_t tile_index = base_tile_index + (effective_line / 8);
        int row_in_tile = effective_line % 8;
        uint16_t tile_addr = 0x8000 + tile_index * 16;

        for (int x = 0; x < 8; ++x) {
            int px = xflip ? (7 - x) : x;
            int screen_x = sprite.x + x - 8; // hardware X offset
            if (screen_x < 0 || screen_x >= 160) continue;
            uint8_t pixel = get_tile_pixel(mmu, tile_addr, px, row_in_tile);
            if (pixel == 0) continue;
            if (behind_bg && bgwin_pixel_ids[ly * 160 + screen_x] != 0) continue;
            framebuffer[ly * 160 + screen_x] = get_color(pixel, palette);
        }
    }

    // TODO: OAM/VRAM locking: 在 mode 2/3 時，MMU 應禁止 CPU 存取 OAM/VRAM 區域
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
        0xFFCADFAA, // Slightly lighter green
        0xFF8EBF60, // Light green
        0xFF305030  // Dark green (darker for contrast tests)
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
