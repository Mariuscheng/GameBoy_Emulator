/// 顏色定義
#[derive(Debug, Clone, Copy, PartialEq)]
pub struct Color {
    pub r: u8,
    pub g: u8,
    pub b: u8,
    pub a: u8,
}

/// 像素操作 trait
pub trait Pixel {
    fn rgba(&self) -> [u8; 4];
}

impl Color {
    pub const fn new(r: u8, g: u8, b: u8, a: u8) -> Self {
        Self { r, g, b, a }
    }

    // GameBoy原始綠色調色盤
    pub const TRANSPARENT: Self = Self::new(0, 0, 0, 0);
    pub const WHITE: Self = Self::new(155, 188, 15, 255); // 最淺的綠色
    pub const LIGHT_GRAY: Self = Self::new(139, 172, 15, 255); // 淺綠色
    pub const DARK_GRAY: Self = Self::new(48, 98, 48, 255); // 深綠色
    pub const BLACK: Self = Self::new(15, 56, 15, 255); // 最深的綠色
}

impl Pixel for Color {
    fn rgba(&self) -> [u8; 4] {
        [self.r, self.g, self.b, self.a]
    }
}

impl From<u8> for Color {
    fn from(gb_color: u8) -> Self {
        match gb_color & 0x03 {
            0 => Color::WHITE,
            1 => Color::LIGHT_GRAY,
            2 => Color::DARK_GRAY,
            3 => Color::BLACK,
            _ => unreachable!(),
        }
    }
}

#[derive(Debug, Clone, Copy)]
pub struct BgPixel {
    pub color: Color,
    color_number: u8, // 原始顏色編號（0-3）
}

impl BgPixel {
    pub fn new(color_idx: u8, color_number: u8) -> Self {
        Self {
            color: Color::from(color_idx),
            color_number,
        }
    }

    pub fn color_number(&self) -> u8 {
        self.color_number
    }
}

impl From<Color> for BgPixel {
    fn from(color: Color) -> Self {
        BgPixel {
            color,
            color_number: 0, // 預設為0，因為我們無法從 Color 反推出原始顏色編號
        }
    }
}

impl Pixel for BgPixel {
    fn rgba(&self) -> [u8; 4] {
        self.color.rgba()
    }
}
