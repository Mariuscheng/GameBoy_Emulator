#ifndef PPU_H
#define PPU_H

#include <cstdint>
#include <array>
#include <vector>

class MMU; // Forward declaration

class PPU {
public:
    PPU();
    ~PPU();

        void step(int cycles, MMU& mmu);
        void render_scanline(MMU& mmu, uint8_t shadow_scx, uint8_t shadow_scy);
    const std::array<uint32_t, 160 * 144>& get_framebuffer() const { return framebuffer; }

    // LCD Control Register (0xFF40)
    uint8_t get_lcdc() const;
    void set_lcdc(uint8_t value);

    // LCD Status Register (0xFF41)
    uint8_t get_stat() const;
    void set_stat(uint8_t value);

    // Scroll registers
    uint8_t get_scy() const { return scy; }
    void set_scy(uint8_t value) { scy = value; }
    uint8_t get_scx() const { return scx; }
    void set_scx(uint8_t value) { scx = value; }

    // LY register (current scanline)
    uint8_t get_ly() const { return ly; }
    void set_ly(uint8_t value) { ly = value; win_line_counter = 0; }

    // LYC register (LY compare)
    uint8_t get_lyc() const { return lyc; }
    void set_lyc(uint8_t value) { lyc = value; }

    // Window position
    uint8_t get_wy() const { return wy; }
    void set_wy(uint8_t value) { wy = value; }
    uint8_t get_wx() const { return wx; }
    void set_wx(uint8_t value) { wx = value; }

    // Palettes
    uint8_t get_bgp() const { return bgp; }
    void set_bgp(uint8_t value) { bgp = value; }
    uint8_t get_obp0() const { return obp0; }
    void set_obp0(uint8_t value) { obp0 = value; }
    uint8_t get_obp1() const { return obp1; }
    void set_obp1(uint8_t value) { obp1 = value; }

    // Debug / tuning helper: set adjustable LCD start cycle offset used when LCDC bit7 transitions from 0->1
    void set_lcd_start_cycle_offset(uint16_t offset) { lcd_start_cycle_offset = offset; }

    // Debug summary dump for LCDC ON events (#3 requirement)
    void dump_lcd_on_summary() const;

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

    // LCD registers
    uint8_t lcdc; // LCD Control
    uint8_t stat; // LCD Status
    uint8_t scy, scx; // Scroll Y, X
    uint8_t ly; // Current scanline
    uint8_t lyc; // LY Compare
    uint8_t wy, wx; // Window Y, X position
    uint8_t bgp; // Background palette
    uint8_t obp0, obp1; // Object palettes

    // Timing
    uint16_t cycle_count;
    // Current PPU mode: 0=HBlank, 1=VBlank, 2=OAM, 3=Transfer
    uint8_t ppu_mode;
    // Shadow registers for timing-accurate rendering
    uint8_t shadow_scx, shadow_scy;
    // Window internal line counter: increments only on lines where window is visible (WX <= 166 and LY >= WY)
    uint16_t win_line_counter = 0;
    // One-time debug print flag
    bool frame_info_printed = false;
    bool lcd_was_on = false; // track previous LCDC bit7 state for edge detection
    long off_cycle_counter = 0; // accumulate cycles while LCD is off; used to derive initial scanline offset when turned back on
    uint64_t global_cycles = 0; // total PPU cycles for diagnostics
    bool log_after_lcd_on = false; // enable per-cycle logging just after LCD is enabled
    int lcd_on_log_cycles_remaining = 0; // remaining cycles to log after on
    uint16_t lcd_start_cycle_offset = 0; // 可調整的 LCD 開啟初始掃描線週期偏移 (手動搜尋用)
    std::vector<LcdOnEvent> lcd_on_events; // 收集所有 LCDC ON 事件摘要
    uint16_t display_cycle_offset = 0; // 新：記錄使用者要求的顯示偏移，不再影響模式時序，只做診斷
    bool pending_lcd_enable = false; // flag to delay LCD enable by 1 cycle
    // Removed delayed LCD enable scheduling fields; kept for reference (alignment now immediate).

    // --- OAM bug support ---
    // During Mode 2 (OAM search), hardware reads Y and X (first two bytes) of each sprite entry.
    // Each sprite consumes 2 cycles (80 cycles total for 40 sprites). We expose the pair base
    // currently being fetched for corruption source and freeze the last pair when entering Mode 3.
    uint16_t oam_search_pair_base = 0xFE00; // current pair base in Mode2 (Y/X of sprite n)
    uint16_t oam_last_mode2_pair_base = 0xFE00; // frozen source used during Mode3

public:
    uint16_t get_oam_search_pair_base() const { return oam_search_pair_base; }
    uint16_t get_oam_last_mode2_pair_base() const { return oam_last_mode2_pair_base; }

    // Helper functions
    uint32_t get_color(uint8_t color_id, uint8_t palette) const;
    void render_background(MMU& mmu, uint8_t shadow_scx, uint8_t shadow_scy);
    void render_window(MMU& mmu, uint8_t shadow_scx, uint8_t shadow_scy);
    void render_sprites(MMU& mmu);
    uint8_t get_tile_pixel(MMU& mmu, uint16_t tile_addr, uint8_t x, uint8_t y) const;
};

#endif // PPU_H