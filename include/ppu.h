#ifndef PPU_H
#define PPU_H

#include <cstdint>
#include <array>
#include <vector>
#include "sprite_renderer.h"
#include "background_renderer.h"
#include "lcd_controller.h"

class MMU; // Forward declaration

class PPU {
public:
    PPU();
    ~PPU();

        void step(int cycles, MMU& mmu);
        void render_scanline(MMU& mmu, uint8_t shadow_scx, uint8_t shadow_scy);
    const std::array<uint32_t, 160 * 144>& get_framebuffer() const { return framebuffer; }

    // LCD Control Register (0xFF40)
    uint8_t get_lcdc() const { return lcd_controller.get_lcdc(); }
    void set_lcdc(uint8_t value) { lcd_controller.set_lcdc(value); }

    // LCD Status Register (0xFF41)
    uint8_t get_stat() const { return lcd_controller.get_stat(); }
    void set_stat(uint8_t value) { lcd_controller.set_stat(value); }

    // Scroll registers
    uint8_t get_scy() const { return lcd_controller.get_scy(); }
    void set_scy(uint8_t value) { lcd_controller.set_scy(value); }
    uint8_t get_scx() const { return lcd_controller.get_scx(); }
    void set_scx(uint8_t value) { lcd_controller.set_scx(value); }

    // LY register (current scanline)
    uint8_t get_ly() const { return lcd_controller.get_ly(); }
    void set_ly(uint8_t value) { lcd_controller.set_ly(value); }

    // LYC register (LY compare)
    uint8_t get_lyc() const { return lcd_controller.get_lyc(); }
    void set_lyc(uint8_t value) { lcd_controller.set_lyc(value); }

    // Window position
    uint8_t get_wy() const { return lcd_controller.get_wy(); }
    void set_wy(uint8_t value) { lcd_controller.set_wy(value); }
    uint8_t get_wx() const { return lcd_controller.get_wx(); }
    void set_wx(uint8_t value) { lcd_controller.set_wx(value); }

    // Palettes
    uint8_t get_bgp() const { return lcd_controller.get_bgp(); }
    void set_bgp(uint8_t value) { lcd_controller.set_bgp(value); }
    uint8_t get_obp0() const { return lcd_controller.get_obp0(); }
    void set_obp0(uint8_t value) { lcd_controller.set_obp0(value); }
    uint8_t get_obp1() const { return lcd_controller.get_obp1(); }
    void set_obp1(uint8_t value) { lcd_controller.set_obp1(value); }

    // Debug / tuning helper: set adjustable LCD start cycle offset used when LCDC bit7 transitions from 0->1
    void set_lcd_start_cycle_offset(uint16_t offset) { lcd_controller.set_lcd_start_cycle_offset(offset); }
    // Set pending LCD enable delay for sync tests
    void set_pending_lcd_enable_delay(int delay) { lcd_controller.set_pending_lcd_enable_delay(delay); }
    // Set display cycle offset for sync tests
    void set_display_cycle_offset(uint16_t offset) { lcd_controller.set_display_cycle_offset(offset); }

    // Debug summary dump for LCDC ON events (#3 requirement)
    void dump_lcd_on_summary() const { lcd_controller.dump_lcd_on_summary(); }

    struct LcdOnEvent {
        uint64_t global_cycles_at_on; // global PPU cycles when LCDC bit7 rose
        uint16_t start_cycle_count;   // internal cycle_count at enable (now always 0)
        uint16_t applied_offset;      // user requested offset (for diagnostics only)
        uint8_t initial_mode;         // mode right after enabling
        uint8_t ly_at_on;             // LY at enable (should be 0 per spec)
        uint16_t first_mode3_cycle;   // cycle_count value when mode3 first observed (0 if never)
        bool mode3_recorded;          // internal flag
        uint32_t off_cycles_before_on; // number of PPU cycles accumulated while LCD was off prior to enabling
    };

    // (Removed delayed LCD start alignment helper; now immediate enable with modulo alignment.)

private:
    // Framebuffer (160x144 pixels, 32-bit color)
    std::array<uint32_t, 160 * 144> framebuffer;
    // Raw background/window pixel color IDs (0..3) for priority checks
    std::array<uint8_t, 160 * 144> bgwin_pixel_ids;

    // Timing
    uint16_t cycle_count;
    // Current PPU mode: 0=HBlank, 1=VBlank, 2=OAM, 3=Transfer
    uint8_t ppu_mode;
    // Shadow registers for timing-accurate rendering
    uint8_t shadow_scx, shadow_scy;
    // One-time debug print flag
    bool frame_info_printed = false;
    uint64_t global_cycles = 0; // total PPU cycles for diagnostics
    // Removed delayed LCD enable scheduling fields; kept for reference (alignment now immediate).

    // --- OAM bug support ---
    // During Mode 2 (OAM search), hardware reads Y and X (first two bytes) of each sprite entry.
    // Each sprite consumes 2 cycles (80 cycles total for 40 sprites). We expose the pair base
    // currently being fetched for corruption source and freeze the last pair when entering Mode 3.
    uint16_t oam_search_pair_base = 0xFE00; // current pair base in Mode2 (Y/X of sprite n)
    uint16_t oam_last_mode2_pair_base = 0xFE00; // frozen source used during Mode3

    // Sprite renderer component
    SpriteRenderer sprite_renderer;

    // Background renderer component
    BackgroundRenderer background_renderer;

    // LCD controller component
    LCDController lcd_controller;

public:
    uint16_t get_oam_search_pair_base() const { return oam_search_pair_base; }
    uint16_t get_oam_last_mode2_pair_base() const { return oam_last_mode2_pair_base; }
    // Quick timing helper: expose current PPU cycle modulo 4 (for Route A alignment)
    uint8_t get_cycle_mod4() const { return static_cast<uint8_t>(cycle_count & 0x3); }
    uint64_t get_global_cycles() const { return global_cycles; }

    // Helper functions
    uint32_t get_color(uint8_t color_id, uint8_t palette) const;
};

#endif // PPU_H