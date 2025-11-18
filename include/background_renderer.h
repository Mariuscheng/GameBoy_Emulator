#ifndef BACKGROUND_RENDERER_H
#define BACKGROUND_RENDERER_H

#include <cstdint>
#include <array>

class MMU; // Forward declaration

class BackgroundRenderer {
public:
    BackgroundRenderer();
    ~BackgroundRenderer();

    // Render background layer
    void render_background(MMU& mmu, uint8_t ly, uint8_t lcdc, uint8_t scx, uint8_t scy, uint8_t bgp,
                          std::array<uint32_t, 160 * 144>& framebuffer,
                          std::array<uint8_t, 160 * 144>& bgwin_pixel_ids);

    // Render window layer
    void render_window(MMU& mmu, uint8_t ly, uint8_t lcdc, uint8_t wy, uint8_t wx, uint8_t bgp,
                      uint16_t win_line_counter,
                      std::array<uint32_t, 160 * 144>& framebuffer,
                      std::array<uint8_t, 160 * 144>& bgwin_pixel_ids);

    // Get pixel color from tile data
    uint8_t get_tile_pixel(MMU& mmu, uint16_t tile_addr, uint8_t x, uint8_t y) const;

    // Get color from palette
    uint32_t get_color(uint8_t color_id, uint8_t palette) const;

private:
    // Window debug state
    static int window_debug_lines_printed;
    static int last_frame_ly;
    static bool oam_dumped;
};

#endif // BACKGROUND_RENDERER_H