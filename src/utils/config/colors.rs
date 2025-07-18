use sdl3::pixels::Color;

/// GameBoy 的基本顏色配置
pub struct GameBoyColors {
    pub white: Color,
    pub light_gray: Color,
    pub dark_gray: Color,
    pub black: Color,
}

impl Default for GameBoyColors {
    fn default() -> Self {
        Self {
            white: Color::RGB(255, 255, 255),      // 最亮
            light_gray: Color::RGB(192, 192, 192), // 亮灰
            dark_gray: Color::RGB(96, 96, 96),     // 暗灰
            black: Color::RGB(0, 0, 0),            // 最暗
        }
    }
}

/// 獲取默認的 GameBoy 顏色配置
pub fn get_default_colors() -> GameBoyColors {
    GameBoyColors::default()
}
