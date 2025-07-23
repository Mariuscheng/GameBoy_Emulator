// Game Boy Emulator 基本功能測試
// 執行 cargo test 可驗證 CPU/MMU/PPU/APU 初始化與主迴圈

#[cfg(test)]
mod tests {
    use crate::core::apu::APU;
    use crate::core::cpu::cpu::CPU;
    use crate::core::mmu::mmu::MMU;
    use crate::core::ppu::ppu::PPU;

    #[test]
    fn test_cpu_mmu_ppu_apu_init() {
        let mut mmu = MMU::default();
        let mut cpu = CPU {
            ime: false,
            registers: Default::default(),
            mmu: &mut mmu as *mut MMU,
            sdl_backend: None, // Add this line; adjust value if needed
        };
        let mut ppu = PPU {
            mmu: &mut mmu as *mut MMU,
            lcd_enabled: true,
            framebuffer: vec![0; 160 * 144],
            bgp: 0,
            obp0: 0,
            obp1: 0,
            scx: 0,
            scy: 0,
            wy: 0,
            wx: 0,
            lcdc: 0,
            last_frame_time: std::time::Instant::now(),
            fps_counter: 0,
            mode: 0,
            ly: 0,
            lyc: 0,
            stat: 0,
            dots: 0,
            oam: [0; 0xA0],
            vram: std::ptr::null_mut(),
        };
        let mut apu = APU::new();
        // 基本 step/mix 測試
        cpu.step();
        ppu.step();
        apu.mix(44100, 16);
    }
}
