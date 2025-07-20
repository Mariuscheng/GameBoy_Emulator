pub struct MMU {
    // 你可以在這裡加上記憶體欄位，例如：
    pub memory: [u8; 0x10000], // 64KB GameBoy 記憶體
}

impl MMU {
    pub fn new() -> Self {
        MMU {
            memory: [0; 0x10000],
        }
    }
    // 你可以在這裡加上 read_byte/write_byte 等方法
}
