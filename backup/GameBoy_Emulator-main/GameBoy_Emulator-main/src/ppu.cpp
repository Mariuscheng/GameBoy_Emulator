#include "ppu.h"
#include "mmu.h"
#include <iostream>
#include <algorithm>

PPU::PPU() : cycle_count(0), shadow_scx(0), shadow_scy(0), ppu_mode(0) {
    // Initialize registers (start with LCD off so test ROMs control enable timing)
    lcdc = 0x00; // Bit7 off
    stat = 0x80; // Bit7 always 1, mode bits 0
    scy = scx = 0;
    ly = 0;
    lyc = 0;
    wy = wx = 0;
    bgp = 0xFC;
    obp0 = 0xFF;
    obp1 = 0xFF;

    framebuffer.fill(0xFFFFFFFF); // White background (will be overwritten per scanline)
    bgwin_pixel_ids.fill(0);      // All background pixels start as color 0

    // OAM bug scan tracking init
    oam_search_pair_base = 0xFE00;
    oam_last_mode2_pair_base = 0xFE00;
    
    // Initialize pending flags
    pending_lcd_enable = false;
}

PPU::~PPU() {
}

void PPU::step(int cycles, MMU& mmu) {
    for (int i = 0; i < cycles; ++i) {
        global_cycles++; // 全域 PPU 週期計數（包含 LCD 關閉期間）
        
        // Check for pending LCD enable with programmable delay (default 4 T-cycles)
        if (pending_lcd_enable) {
            if (pending_lcd_enable_delay > 0) {
                pending_lcd_enable_delay--; // wait precise T-cycles
            }
            if (pending_lcd_enable_delay == 0) {
                pending_lcd_enable = false;
                cycle_count = 0; // Reset cycle_count when LCD actually turns on
                ppu_mode = 2;
                stat = (stat & ~0x03) | 0x02;
                std::cout << "[PPU] LCDC ON activated gcy=" << global_cycles << " ly=" << (int)ly << " cyc=" << cycle_count << " mode=2" << std::endl;
            }
        }
        
        // 如果 LCD 關閉：依規格 LY 固定為 0，停止模式循環與渲染/中斷
        if (!(lcdc & 0x80)) {
            // LCD 關閉：保持 LY=0，不進行模式循環，但仍然累積關閉期間的 CPU 週期以便重啟時對齊偏移
            ly = 0;
            ppu_mode = 0; // 靜止狀態視作 mode 0
            stat = (stat & ~0x03) | 0x00;
            off_cycle_counter++; // 保留偏移
            continue; // 不執行渲染或中斷
        }
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
                // Freeze last mode2 pair as corruption source for duration of mode3
                oam_last_mode2_pair_base = oam_search_pair_base;
                // 若最近一次 LCDC ON 事件尚未記錄第一個 Mode3 週期，則記錄
                if (!lcd_on_events.empty()) {
                    LcdOnEvent &ev = lcd_on_events.back();
                    if (!ev.mode3_recorded) {
                        ev.first_mode3_cycle = cycle_count;
                        ev.mode3_recorded = true;
                    }
                }
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

        // OAM search pair tracking during Mode2 (each 2 cycles processes one sprite Y/X)
        if (ppu_mode == 2) {
            // cycle_count is cycles elapsed so far in this scanline BEFORE increment below.
            // Map 0..79 -> sprite index 0..39 (2 cycles per sprite). Pair base uses Y/X (first two bytes) at FE00 + i*4.
            // During the first cycle of each pair, we read Y; during the second, we read X.
            // For corruption purposes, we need to track which sprite pair is currently being accessed.
            uint8_t sprite_index = (cycle_count < 80) ? (uint8_t)(cycle_count / 2) : 39;
            oam_search_pair_base = 0xFE00 + sprite_index * 4;
        } else if (ppu_mode != 3) {
            // Outside Mode 2 and 3, reset to base to avoid stale corruption source
            oam_search_pair_base = 0xFE00;
        }

        cycle_count++;

        if (log_after_lcd_on && lcd_on_log_cycles_remaining > 0) {
            std::cout << "[PPU][LCD ON TRACE] gcy=" << global_cycles
                      << " ly=" << (int)ly << " cyc=" << cycle_count
                      << " mode=" << (int)ppu_mode << std::endl;
            lcd_on_log_cycles_remaining--;
            if (lcd_on_log_cycles_remaining == 0) {
                log_after_lcd_on = false;
            }
        }

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

            // Window line counter increments only on lines where the window is actually visible
            // Conditions: window enabled, in visible area, LY >= WY, WX <= 166
            // We check the line that just finished (old_ly), so the increment applies for next line's rendering
            if ((lcdc & 0x20) && old_ly < 144 && old_ly >= wy && wx <= 166) {
                if (win_line_counter < 0xFFFF) win_line_counter++;
            }

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

                // Reset window line counter at start of new frame
                win_line_counter = 0;

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

    // 擴充除錯：前 12 個 window 行 (從 ly==wy 起) 列印右側 6 個 tile id 與 WY/WX
    // 每一幀重新列印前 12 window 行的右側 tile。當 ly==0 (新幀開始) 時重置計數。
    static int window_debug_lines_printed = 0;
    static int last_frame_ly = -1;
    if (ly == 0 && last_frame_ly != 0) { // 新幀開始（LY 回到 0）
        window_debug_lines_printed = 0;
    }
    last_frame_ly = ly;

    if (ly >= wy && window_debug_lines_printed < 12) {
        int win_line = static_cast<int>(win_line_counter); // window 內部行（使用內部行計數器）
        // 右側顯示範圍：tile_x 14..19 (總寬 20 tiles)，集中觀察右下異常區
        std::cout << "[PPU] WIN line=" << win_line << " LY=" << (int)ly
                  << " WY=" << (int)wy << " WX=" << (int)wx
                  << " win_x=" << (int)win_x << " tile_y=" << (win_line/8) << " : ";
        for (int tx = 14; tx < 20; ++tx) {
            int ty = win_line / 8;
            uint16_t addr = win_tile_map + ty * 32 + tx;
            std::cout << std::hex << (int)mmu.ppu_read(addr) << ' ';
        }
        std::cout << std::dec << std::endl;
    window_debug_lines_printed++;

        // 一次性 OAM dump：第一個 window 行時列出所有 sprite 參數以檢查是否出現預期圖樣
        static bool oam_dumped = false;
        if (!oam_dumped && win_line == 0) {
            std::cout << "[PPU] OAM dump (index:y x tile attr)" << std::endl;
            for (int i = 0; i < 40; ++i) {
                uint16_t oam_addr = 0xFE00 + i * 4;
                uint8_t sy = mmu.ppu_read(oam_addr);
                uint8_t sx = mmu.ppu_read(oam_addr + 1);
                uint8_t st = mmu.ppu_read(oam_addr + 2);
                uint8_t sa = mmu.ppu_read(oam_addr + 3);
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
        Sprite sprite{ mmu.ppu_read(oam_addr), mmu.ppu_read(oam_addr + 1),
                   mmu.ppu_read(oam_addr + 2), mmu.ppu_read(oam_addr + 3) };
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
    uint8_t byte1 = mmu.ppu_read(row_addr);
    uint8_t byte2 = mmu.ppu_read(row_addr + 1);

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
    uint8_t prev = lcdc;
    bool turning_on = !(lcdc & 0x80) && (value & 0x80);
    bool turning_off = (lcdc & 0x80) && !(value & 0x80);
    bool window_enable_0_to_1 = !(lcdc & 0x20) && (value & 0x20);
    lcdc = value;
    std::cout << "[PPU] LCDC write prev=" << std::hex << (int)prev << " new=" << (int)value
              << std::dec << " ly=" << (int)ly << " cyc=" << cycle_count
              << (turning_on?" (ON edge)":"") << (turning_off?" (OFF edge)":"") << std::endl;
    if (turning_on) {
        // LCD turning on: delay activation by 4 T-cycles to match hardware sync
        pending_lcd_enable = true;
        pending_lcd_enable_delay = 4; // 1 M-cycle (4 dots)
        ly = 0;
        // Do NOT reset cycle_count here - let it reset when LCD actually activates
        ppu_mode = 0; // Temporary mode
        stat = (stat & ~0x03) | 0x00;
        std::cout << "[PPU] LCDC ON pending gcy=" << global_cycles << std::endl;
    } else if (turning_off) {
        // LCD 關閉：LY 立即歸 0；停止行內累積但保留偏移計時器於 0
        ly = 0;
        cycle_count = 0;
        ppu_mode = 0;
        stat = (stat & ~0x03) | 0x00;
        off_cycle_counter = 0;
        win_line_counter = 0;
    }
    // Reset window line counter when Window Enable bit goes 0->1
    if (window_enable_0_to_1) {
        win_line_counter = 0;
    }
}

void PPU::dump_lcd_on_summary() const {
    if (lcd_on_events.empty()) {
        // std::cout << "[PPU][LCD ON SUMMARY] (no events)" << std::endl;
        return;
    }
    std::cout << "[PPU][LCD ON SUMMARY] count=" << lcd_on_events.size() << std::endl;
    std::cout << " idx | gcy_on | start_cyc | applied_offset | ly_on | init_mode | first_mode3_cyc | mode3_delta | off_cycles_before_on" << std::endl;
    for (size_t i = 0; i < lcd_on_events.size(); ++i) {
        const auto &ev = lcd_on_events[i];
        int delta = ev.mode3_recorded ? (int)ev.first_mode3_cycle - (int)ev.start_cycle_count : -1;
        std::cout << "  " << i
                  << " | " << ev.global_cycles_at_on
                  << " | " << ev.start_cycle_count
                  << " | " << ev.applied_offset
                  << " | " << (int)ev.ly_at_on
                  << " | " << (int)ev.initial_mode
                  << " | " << (ev.mode3_recorded ? std::to_string(ev.first_mode3_cycle) : std::string("(none)"))
                  << " | " << (ev.mode3_recorded ? std::to_string(delta) : std::string("(n/a)"))
                  << " | " << ev.off_cycles_before_on
                  << std::endl;
    }
}

uint8_t PPU::get_stat() const {
    return stat | 0x80; // Bit 7 always set
}

void PPU::set_stat(uint8_t value) {
    stat = (stat & 0x87) | (value & 0x78); // Preserve read-only bits
}
