#ifndef SPRITE_RENDERER_H
#define SPRITE_RENDERER_H

#include <cstdint>
#include <vector>
#include <array>

class MMU; // Forward declaration

class SpriteRenderer {
public:
    SpriteRenderer();
    ~SpriteRenderer();

    // Sprite structure for rendering
    struct Sprite {
        uint8_t y, x, tile, attributes;
        uint8_t oam_index; // Add OAM index for priority resolution

        bool is_on_scanline(uint8_t ly, uint8_t sprite_height) const {
            return ly + 16 >= y && ly + 16 < y + sprite_height;
        }

        bool is_visible() const {
            return y != 0; // Only Y=0 sprites are invisible, X=0 sprites are visible (appear at left edge)
        }
    };

    // Main sprite rendering function
    void render_sprites(MMU& mmu, uint8_t ly, uint8_t lcdc, uint8_t obp0, uint8_t obp1,
                       std::array<uint32_t, 160 * 144>& framebuffer,
                       const std::array<uint8_t, 160 * 144>& bgwin_pixel_ids);

private:
    // Helper functions
    std::vector<Sprite> collect_sprites_on_line(MMU& mmu, uint8_t ly, uint8_t lcdc) const;
    void render_sprite_pixels(MMU& mmu, const std::vector<Sprite>& sprites_on_line,
                             uint8_t ly, uint8_t lcdc, uint8_t obp0, uint8_t obp1,
                             std::array<uint32_t, 160 * 144>& framebuffer,
                             const std::array<uint8_t, 160 * 144>& bgwin_pixel_ids);
    uint8_t get_tile_pixel(MMU& mmu, uint16_t tile_addr, uint8_t x, uint8_t y) const;
    uint32_t get_color(uint8_t color_id, uint8_t palette) const;
};

#endif // SPRITE_RENDERER_H