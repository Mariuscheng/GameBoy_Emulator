#ifndef LCD_CONTROLLER_H
#define LCD_CONTROLLER_H

#include <cstdint>
#include <vector>

class LCDController {
public:
    LCDController();
    ~LCDController();

    // LCD Control Register (0xFF40) with context for state management
    uint8_t get_lcdc() const { return lcdc; }
    void set_lcdc(uint8_t value) { lcdc = value; }
    void set_lcdc_with_context(uint8_t value, uint64_t global_cycles, uint16_t cycle_count, uint8_t& ly_ref, uint8_t& ppu_mode_ref, uint8_t& stat_ref, uint16_t& win_line_counter_ref);

    // LCD Status Register (0xFF41)
    uint8_t get_stat() const { return stat; }
    void set_stat(uint8_t value) { stat = value; }

    // Scroll registers
    uint8_t get_scy() const { return scy; }
    void set_scy(uint8_t value) { scy = value; }
    uint8_t get_scx() const { return scx; }
    void set_scx(uint8_t value) { scx = value; }

    // LY register (current scanline)
    uint8_t get_ly() const { return ly; }
    void set_ly(uint8_t value) { ly = value; }

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

    // Window line counter access
    uint16_t get_win_line_counter() const { return win_line_counter; }
    void set_win_line_counter(uint16_t value) { win_line_counter = value; }
    void increment_win_line_counter() { win_line_counter++; }

    // Debug / tuning helpers
    void set_lcd_start_cycle_offset(uint16_t offset) { lcd_start_cycle_offset = offset; }
    void set_pending_lcd_enable_delay(int delay) { pending_lcd_enable_delay = delay; }
    void set_display_cycle_offset(uint16_t offset) { display_cycle_offset = offset; }

    // LCD state tracking
    bool get_lcd_was_on() const { return lcd_was_on; }
    void set_lcd_was_on(bool value) { lcd_was_on = value; }
    long get_off_cycle_counter() const { return off_cycle_counter; }
    void set_off_cycle_counter(long value) { off_cycle_counter = value; }
    void increment_off_cycle_counter() { off_cycle_counter++; }

    // LCD enable pending processing
    void process_pending_lcd_enable(uint64_t global_cycles, uint16_t& cycle_count_ref, uint8_t& ppu_mode_ref);

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

    // LCD ON event tracking
    void add_lcd_on_event(const LcdOnEvent& event);
    const std::vector<LcdOnEvent>& get_lcd_on_events() const { return lcd_on_events; }

    // Debug logging control
    bool get_log_after_lcd_on() const { return log_after_lcd_on; }
    void set_log_after_lcd_on(bool value) { log_after_lcd_on = value; }
    int get_lcd_on_log_cycles_remaining() const { return lcd_on_log_cycles_remaining; }
    void set_lcd_on_log_cycles_remaining(int value) { lcd_on_log_cycles_remaining = value; }
    void decrement_lcd_on_log_cycles_remaining() { if (lcd_on_log_cycles_remaining > 0) lcd_on_log_cycles_remaining--; }

private:
    // LCD registers
    uint8_t lcdc; // LCD Control
    uint8_t stat; // LCD Status
    uint8_t scy, scx; // Scroll Y, X
    uint8_t ly; // Current scanline
    uint8_t lyc; // LY Compare
    uint8_t wy, wx; // Window Y, X position
    uint8_t bgp; // Background palette
    uint8_t obp0, obp1; // Object palettes

    // Window internal line counter: increments only on lines where window is visible (WX <= 166 and LY >= WY)
    uint16_t win_line_counter = 0;

    // LCD state tracking
    bool lcd_was_on = false; // track previous LCDC bit7 state for edge detection
    long off_cycle_counter = 0; // accumulate cycles while LCD is off; used to derive initial scanline offset when turned back on

    // LCD enable pending state
    bool pending_lcd_enable = false; // flag to delay LCD enable by 1 cycle
    int pending_lcd_enable_delay = 0; // remaining T-cycles before enabling (for sync tests)

    // Debug / tuning
    uint16_t lcd_start_cycle_offset = 0; // 可調整的 LCD 開啟初始掃描線週期偏移 (手動搜尋用)
    uint16_t display_cycle_offset = 0; // 新：記錄使用者要求的顯示偏移，不再影響模式時序，只做診斷

    // LCD ON event tracking for diagnostics
    std::vector<LcdOnEvent> lcd_on_events;

    // Debug logging control
    bool log_after_lcd_on = false; // enable per-cycle logging just after LCD is enabled
    int lcd_on_log_cycles_remaining = 0; // remaining cycles to log after on
};

#endif // LCD_CONTROLLER_H