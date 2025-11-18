#include "sprite_renderer.h"
#include "mmu.h"
#include <algorithm>

SpriteRenderer::SpriteRenderer() {
}

SpriteRenderer::~SpriteRenderer() {
}

void SpriteRenderer::render_sprites(MMU& mmu, uint8_t ly, uint8_t lcdc, uint8_t obp0, uint8_t obp1,
                                   std::array<uint32_t, 160 * 144>& framebuffer,
                                   const std::array<uint8_t, 160 * 144>& bgwin_pixel_ids) {
    // Sprite rendering (OAM search and rendering)
    // GameBoy supports up to 40 sprites, but only 10 per scanline

    auto sprites_on_line = collect_sprites_on_line(mmu, ly, lcdc);
    if (!sprites_on_line.empty()) {
        // 正確排序：X 較小優先，X 相同時 OAM index 較小優先
        std::sort(sprites_on_line.begin(), sprites_on_line.end(), [](const Sprite& a, const Sprite& b) {
            if (a.x == b.x) return a.oam_index < b.oam_index;
            return a.x < b.x;
        });
        render_sprite_pixels(mmu, sprites_on_line, ly, lcdc, obp0, obp1, framebuffer, bgwin_pixel_ids);
    }

    // TODO: OAM/VRAM locking: 在 mode 2/3 時，MMU 應禁止 CPU 存取 OAM/VRAM 區域
}

std::vector<SpriteRenderer::Sprite> SpriteRenderer::collect_sprites_on_line(MMU& mmu, uint8_t ly, uint8_t lcdc) const {
    std::vector<Sprite> sprites_on_line;
    uint8_t sprite_height_global = (lcdc & 0x04) ? 16 : 8;

    // OAM order scanning: take first 10 sprites appearing on this line
    for (int i = 0; i < 40 && sprites_on_line.size() < 10; ++i) {
        uint16_t oam_addr = 0xFE00 + i * 4;
        uint8_t attributes = mmu.ppu_read(oam_addr + 3);
        
        // No per-sprite hard-coded fixes here (dmg-acid2 special-case removed).
        // Attributes are read directly from OAM and respected by render logic.
        
        Sprite sprite{
            mmu.ppu_read(oam_addr),     // y
            mmu.ppu_read(oam_addr + 1), // x
            mmu.ppu_read(oam_addr + 2), // tile
            attributes,                  // corrected attributes
            static_cast<uint8_t>(i)     // oam_index
        };

        if (!sprite.is_visible()) continue;
        if (sprite.is_on_scanline(ly, sprite_height_global)) {
            sprites_on_line.push_back(sprite);
        }
    }
    return sprites_on_line;
}

void SpriteRenderer::render_sprite_pixels(MMU& mmu, const std::vector<Sprite>& sprites_on_line,
                                         uint8_t ly, uint8_t lcdc, uint8_t obp0, uint8_t obp1,
                                         std::array<uint32_t, 160 * 144>& framebuffer,
                                         const std::array<uint8_t, 160 * 144>& bgwin_pixel_ids) {
    // Pre-compute sprite evaluations for this scanline
    struct SpriteEval {
        const Sprite* sprite;
        uint16_t tile_addr;
        int row_in_tile;
        bool xflip;
        bool behind_bg;
        uint8_t palette;
        int start_x;
    };

    std::vector<SpriteEval> evals;
    evals.reserve(sprites_on_line.size());

    uint8_t sprite_height = (lcdc & 0x04) ? 16 : 8;

    for (const auto& sprite : sprites_on_line) {
        int line_in_sprite = ly - (sprite.y - 16);
        if (line_in_sprite < 0 || line_in_sprite >= sprite_height) continue;

        // Calculate effective line considering Y-flip
        bool yflip = (sprite.attributes & 0x40) != 0;
        int effective_line = yflip ? (sprite_height - 1 - line_in_sprite) : line_in_sprite;

        // Calculate tile index and row
        uint8_t base_tile_index = (sprite_height == 16) ? (sprite.tile & 0xFE) : sprite.tile;
        uint8_t tile_index = base_tile_index + (effective_line / 8);
        int row_in_tile = effective_line % 8;
        uint16_t tile_addr = 0x8000 + tile_index * 16;

        // Extract attributes
        bool xflip = (sprite.attributes & 0x20) != 0;
        bool behind_bg = (sprite.attributes & 0x80) != 0;
        uint8_t palette = (sprite.attributes & 0x10) ? obp1 : obp0;

        // OAM stores X/Y with offsets (X=screen_x + 8, Y=screen_y + 16)
        // Convert to actual on-screen X by subtracting 8
        evals.push_back({
            &sprite,
            tile_addr,
            row_in_tile,
            xflip,
            behind_bg,
            palette,
            static_cast<int>(sprite.x) - 8  // start_x (adjusted)
        });
    }

    // Render pixels: process sprites in OAM order (collected order)
    for (int screen_x = 0; screen_x < 160; ++screen_x) {
        for (const auto& eval : evals) {
            // 檢查 sprite 是否覆蓋此像素
            if (screen_x < eval.start_x || screen_x >= eval.start_x + 8) continue;

            // 取得 sprite tile 的像素
            int px_in_sprite = screen_x - eval.start_x;
            int px = eval.xflip ? (7 - px_in_sprite) : px_in_sprite;
            uint8_t pixel = get_tile_pixel(mmu, eval.tile_addr, static_cast<uint8_t>(px),
                                         static_cast<uint8_t>(eval.row_in_tile));

            // 只有 sprite 像素非 0 才能覆蓋
            if (pixel == 0) continue;

            // Sprite 優先權：behind_bg 僅在背景 tile/pixel id 為 0 時才顯示
            if (eval.behind_bg && bgwin_pixel_ids[ly * 160 + screen_x] != 0) {
                continue;
            }

            // 這個 sprite 贏了，繪製並跳出
            framebuffer[ly * 160 + screen_x] = get_color(pixel, eval.palette);
            break;
        }
    }
}

uint8_t SpriteRenderer::get_tile_pixel(MMU& mmu, uint16_t tile_addr, uint8_t x, uint8_t y) const {
    uint16_t row_addr = tile_addr + y * 2;
    uint8_t byte1 = mmu.ppu_read(row_addr);
    uint8_t byte2 = mmu.ppu_read(row_addr + 1);

    uint8_t bit = 7 - x;
    uint8_t pixel = ((byte1 & (1 << bit)) ? 1 : 0) | ((byte2 & (1 << bit)) ? 2 : 0);

    return pixel;
}

uint32_t SpriteRenderer::get_color(uint8_t color_id, uint8_t palette) const {
    // Extract the color from palette based on color_id
    uint8_t shift = color_id * 2;
    uint8_t color_value = (palette >> shift) & 0x03;

    // Game Boy color palette (simplified grayscale for sprites)
    switch (color_value) {
        case 0: return 0xFFFFFFFF; // White (transparent for sprites, but we handle transparency elsewhere)
        case 1: return 0xFFAAAAAA; // Light gray
        case 2: return 0xFF555555; // Dark gray
        case 3: return 0xFF000000; // Black
        default: return 0xFFFFFFFF;
    }
}