#ifndef PPU_H
#define PPU_H

#include <cstdint>
#include <array>

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
    // One-time debug print flag
    bool frame_info_printed = false;

    // Helper functions
    uint32_t get_color(uint8_t color_id, uint8_t palette) const;
    void render_background(MMU& mmu, uint8_t shadow_scx, uint8_t shadow_scy);
    void render_window(MMU& mmu, uint8_t shadow_scx, uint8_t shadow_scy);
    void render_sprites(MMU& mmu);
    uint8_t get_tile_pixel(MMU& mmu, uint16_t tile_addr, uint8_t x, uint8_t y) const;
};

#endif // PPU_H