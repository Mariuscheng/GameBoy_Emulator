// LCD 寄存器結構體
#[derive(Debug, Default, Clone)]
pub struct LCDRegisters {
    pub lcdc: u8, // LCD Control
    pub stat: u8, // LCD Status
    pub scy: u8,  // Scroll Y
    pub scx: u8,  // Scroll X
    pub ly: u8,   // LCD Y-Coordinate
    pub lyc: u8,  // LY Compare
    pub dma: u8,  // DMA Transfer and Start Address
    pub bgp: u8,  // BG Palette Data
    pub obp0: u8, // Object Palette 0 Data
    pub obp1: u8, // Object Palette 1 Data
    pub wy: u8,   // Window Y Position
    pub wx: u8,   // Window X Position minus 7
}

impl LCDRegisters {
    pub fn new() -> Self {
        Self {
            lcdc: 0x91, // LCD & PPU enabled, BG display on
            stat: 0x00, // Default status
            scy: 0x00,  // No scroll
            scx: 0x00,  // No scroll
            ly: 0x00,   // Start at line 0
            lyc: 0x00,  // No compare value
            dma: 0x00,  // No DMA
            bgp: 0xE4,  // Default GB palette
            obp0: 0x00, // Clear palette
            obp1: 0x00, // Clear palette
            wy: 0x00,   // Window Y = 0
            wx: 0x00,   // Window X = 0
        }
    }
}
