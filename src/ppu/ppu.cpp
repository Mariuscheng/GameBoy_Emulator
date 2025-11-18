#include "ppu.h"
#include "mmu.h"
#include <iostream>
#include <algorithm>

PPU::PPU() : cycle_count(0), shadow_scx(0), shadow_scy(0), ppu_mode(0) {
    framebuffer.fill(0xFFFFFFFF); // White background (will be overwritten per scanline)
    bgwin_pixel_ids.fill(0);      // All background pixels start as color 0

    // OAM bug scan tracking init
    oam_search_pair_base = 0xFE00;
    oam_last_mode2_pair_base = 0xFE00;
}

PPU::~PPU() {
}

void PPU::step(int cycles, MMU& mmu) {
    for (int i = 0; i < cycles; ++i) {
        global_cycles++; // 全域 PPU 週期計數（包含 LCD 關閉期間）
        
        // Check for pending LCD enable with programmable delay (default 4 T-cycles)
        lcd_controller.process_pending_lcd_enable(global_cycles, cycle_count, ppu_mode);
        
        // 如果 LCD 關閉：依規格 LY 固定為 0，停止模式循環與渲染/中斷
        if (!(lcd_controller.get_lcdc() & 0x80)) {
            // LCD 關閉：保持 LY=0，不進行模式循環，但仍然累積關閉期間的 CPU 週期以便重啟時對齊偏移
            lcd_controller.set_ly(0);
            ppu_mode = 0; // 靜止狀態視作 mode 0
            lcd_controller.set_stat((lcd_controller.get_stat() & ~0x03) | 0x00);
            lcd_controller.increment_off_cycle_counter(); // 保留偏移
            continue; // 不執行渲染或中斷
        }
        // Determine mode based on whether we're in VBlank or visible area
        uint8_t new_mode;
        
        if (lcd_controller.get_ly() >= 144) {
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
            lcd_controller.set_stat((lcd_controller.get_stat() & ~0x03) | ppu_mode);

            if (ppu_mode == 2) {
                // Mode 2 (OAM Search)
                if (lcd_controller.get_stat() & 0x20) {
                    uint8_t if_reg = mmu.read_byte(0xFF0F);
                    if_reg |= 0x02;
                    mmu.write_byte(0xFF0F, if_reg);
                }
            } else if (ppu_mode == 3) {
                // Mode 3 (Pixel Transfer) - snapshot scroll values and render
                shadow_scx = lcd_controller.get_scx();
                shadow_scy = lcd_controller.get_scy();
                render_scanline(mmu, shadow_scx, shadow_scy);
                // Freeze last mode2 pair as corruption source for duration of mode3
                oam_last_mode2_pair_base = oam_search_pair_base;
                // 若最近一次 LCDC ON 事件尚未記錄第一個 Mode3 週期，則記錄
                if (!lcd_controller.get_lcd_on_events().empty()) {
                    auto& events = const_cast<std::vector<LCDController::LcdOnEvent>&>(lcd_controller.get_lcd_on_events());
                    LCDController::LcdOnEvent &ev = events.back();
                    if (!ev.mode3_recorded) {
                        ev.first_mode3_cycle = cycle_count;
                        ev.mode3_recorded = true;
                    }
                }
            } else if (ppu_mode == 0) {
                // Mode 0 (HBlank)
                if (lcd_controller.get_stat() & 0x08) {
                    uint8_t if_reg = mmu.read_byte(0xFF0F);
                    if_reg |= 0x02;
                    mmu.write_byte(0xFF0F, if_reg);
                }
            } else if (ppu_mode == 1) {
                // Mode 1 (VBlank) - triggered at LY=144
                if (lcd_controller.get_ly() == 144) {
                    uint8_t if_reg = mmu.read_byte(0xFF0F);
                    if_reg |= 0x01; // VBlank interrupt
                    mmu.write_byte(0xFF0F, if_reg);

                    // STAT mode 1 interrupt
                    if (lcd_controller.get_stat() & 0x10) {
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

        // One-time debug of initial register setup for acid2 layout issues
        if (!frame_info_printed && lcd_controller.get_ly() == 0 && cycle_count == 1) {
            frame_info_printed = true;
        }

        // End of scanline
        if (cycle_count == 456) {
            cycle_count = 0;

            uint8_t old_ly = lcd_controller.get_ly();
            lcd_controller.set_ly(lcd_controller.get_ly() + 1);

            // Window line counter increments only on lines where the window is actually visible
            // Conditions: window enabled, in visible area, LY >= WY, WX <= 166
            // We check the line that just finished (old_ly), so the increment applies for next line's rendering
            if ((lcd_controller.get_lcdc() & 0x20) && old_ly < 144 && old_ly >= lcd_controller.get_wy() && lcd_controller.get_wx() <= 166) {
                lcd_controller.increment_win_line_counter();
            }

            // LYC coincidence
            if (lcd_controller.get_ly() == lcd_controller.get_lyc()) {
                lcd_controller.set_stat(lcd_controller.get_stat() | 0x04);
                if (lcd_controller.get_stat() & 0x40) {
                    uint8_t if_reg = mmu.read_byte(0xFF0F);
                    if_reg |= 0x02;
                    mmu.write_byte(0xFF0F, if_reg);
                }
            } else {
                lcd_controller.set_stat(lcd_controller.get_stat() & ~0x04);
            }

            // Frame reset when LY reaches 154
            if (lcd_controller.get_ly() == 154) {
                lcd_controller.set_ly(0);
                ppu_mode = 2;
                lcd_controller.set_stat((lcd_controller.get_stat() & ~0x03) | 0x02);

                // Reset window line counter at start of new frame
                lcd_controller.set_win_line_counter(0);

                // STAT mode 2 interrupt
                if (lcd_controller.get_stat() & 0x20) {
                    uint8_t if_reg = mmu.read_byte(0xFF0F);
                    if_reg |= 0x02;
                    mmu.write_byte(0xFF0F, if_reg);
                }
            }
        }
    }
}

void PPU::render_scanline(MMU& mmu, uint8_t shadow_scx, uint8_t shadow_scy) {
    if (!(lcd_controller.get_lcdc() & 0x80)) return; // LCD disabled
    if (lcd_controller.get_ly() >= 144) return; // Don't render during VBlank (LY 144-153)

    // Clear scanline with background color ID 0 (palette mapped)
    uint32_t base_color = get_color(0, lcd_controller.get_bgp());
    for (int x = 0; x < 160; ++x) {
        framebuffer[lcd_controller.get_ly() * 160 + x] = base_color;
        bgwin_pixel_ids[lcd_controller.get_ly() * 160 + x] = 0; // raw color id 0
    }

    if (lcd_controller.get_lcdc() & 0x01) background_renderer.render_background(mmu, lcd_controller.get_ly(), lcd_controller.get_lcdc(), shadow_scx, shadow_scy, lcd_controller.get_bgp(), framebuffer, bgwin_pixel_ids);
    if (lcd_controller.get_lcdc() & 0x20) background_renderer.render_window(mmu, lcd_controller.get_ly(), lcd_controller.get_lcdc(), lcd_controller.get_wy(), lcd_controller.get_wx(), lcd_controller.get_bgp(), lcd_controller.get_win_line_counter(), framebuffer, bgwin_pixel_ids);
    if (lcd_controller.get_lcdc() & 0x02) sprite_renderer.render_sprites(mmu, lcd_controller.get_ly(), lcd_controller.get_lcdc(), lcd_controller.get_obp0(), lcd_controller.get_obp1(), framebuffer, bgwin_pixel_ids);
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

