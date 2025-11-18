#include "lcd_controller.h"
#include <iostream>

LCDController::LCDController()
    : lcdc(0), stat(0), scy(0), scx(0), ly(0), lyc(0), wy(0), wx(0),
      bgp(0xFC), obp0(0), obp1(0), win_line_counter(0), lcd_was_on(false),
      off_cycle_counter(0), pending_lcd_enable(false), pending_lcd_enable_delay(0),
      lcd_start_cycle_offset(0), display_cycle_offset(0), log_after_lcd_on(false),
      lcd_on_log_cycles_remaining(0) {
}

LCDController::~LCDController() {
}

void LCDController::set_lcdc_with_context(uint8_t value, uint64_t global_cycles, uint16_t cycle_count, uint8_t& ly_ref, uint8_t& ppu_mode_ref, uint8_t& stat_ref, uint16_t& win_line_counter_ref) {
    uint8_t prev = lcdc;
    bool turning_on = !(lcdc & 0x80) && (value & 0x80);
    bool turning_off = (lcdc & 0x80) && !(value & 0x80);
    bool window_enable_0_to_1 = !(lcdc & 0x20) && (value & 0x20);
    lcdc = value;
    if (turning_on) {
        // LCD turning on: immediate activation for OAM sync tests
        pending_lcd_enable = true;
        pending_lcd_enable_delay = 0; // Immediate
        ly_ref = 0;
        // Do NOT reset cycle_count here - let it reset when LCD actually activates
        ppu_mode_ref = 0; // Temporary mode
        stat_ref = (stat_ref & ~0x03) | 0x00;
    } else if (turning_off) {
        // LCD 關閉：LY 立即歸 0；停止行內累積但保留偏移計時器於 0
        ly_ref = 0;
        // cycle_count = 0; // This should be handled by PPU
        ppu_mode_ref = 0;
        stat_ref = (stat_ref & ~0x03) | 0x00;
        off_cycle_counter = 0;
        win_line_counter_ref = 0;
    }
}

void LCDController::process_pending_lcd_enable(uint64_t global_cycles, uint16_t& cycle_count_ref, uint8_t& ppu_mode_ref) {
    // Check for pending LCD enable with programmable delay (default 4 T-cycles)
    if (pending_lcd_enable) {
        if (pending_lcd_enable_delay > 0) {
            pending_lcd_enable_delay--; // wait precise T-cycles
        }
        if (pending_lcd_enable_delay == 0) {
            pending_lcd_enable = false;
            cycle_count_ref = 0; // Reset cycle_count when LCD actually turns on
            ppu_mode_ref = 2;
            stat = (stat & ~0x03) | 0x02;
        }
    }
}

void LCDController::dump_lcd_on_summary() const {
    // Debug function - removed debug output
}