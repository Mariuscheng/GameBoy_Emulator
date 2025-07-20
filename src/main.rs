#[allow(dead_code)]
#[allow(unused_variables)]
#[allow(unreachable_patterns)]
#[allow(unused_imports)]
#[allow(unused_must_use)]
mod core;
mod interface;
mod utils;

use crate::core::cpu::cpu::CPU;
use crate::core::mmu::MMU;
use crate::core::ppu::ppu::PPU;
use crate::core::ppu::vram::VRAM;
use crate::core::rom_header;
use std::cell::RefCell;
use std::rc::Rc;

fn main() {
    // 顯示 ROM header 資訊（只呼叫一次）
    rom_header::display_rom_header_info();

    let mmu = Rc::new(RefCell::new(MMU::new()));
    // 載入 ROM（假設有 load_rom 方法）
    // mmu.borrow_mut().load_rom(rom_data);

    let mut cpu = CPU::new(mmu.clone());

    // 設定暫存器
    cpu.registers_mut().b = 0b1000_0000;

    // 執行指令（例如 BIT 7,B）
    let _ = crate::core::cpu::bit::dispatch(&mut cpu, 0x78);

    // 初始化 VRAM 與 PPU
    let mut vram = VRAM::new();
    let mut ppu = PPU::new(mmu.clone());

    // 範例：寫入 VRAM 並讀取
    vram.write_byte(0x0000, 0xAB); // 寫入 VRAM 第一格
    let vram_val = vram.read_byte(0x0000);
    println!("VRAM[0x0000] = {:#X}", vram_val);

    // 範例：PPU 設定 LCD 狀態
    ppu.set_lcd_enabled(true);
    println!("PPU LCD 狀態: {}", ppu.is_lcd_enabled());

    // 範例：PPU 執行一個步驟（tick）
    ppu.tick();

    // 最後再初始化 SDL 顯示
    interface::sdl3_display::init_sdl();

    // VRAM 進階測試：清空、邊界
    vram.write_byte(0x1FFF, 0xCD); // 寫入 VRAM 最後一格
    let last_val = vram.read_byte(0x1FFF);
    println!("VRAM[0x1FFF] = {:#X}", last_val);
    vram.write_byte(0x2000, 0xEF); // 超出範圍，應無效
    let out_val = vram.read_byte(0x2000);
    println!("VRAM[0x2000] (out of range) = {:#X}", out_val);
    // 清空 VRAM（改用方法）
    vram.clear();
    println!("VRAM[0x0000] after clear = {:#X}", vram.read_byte(0x0000));
    println!("VRAM[0x1FFF] after clear = {:#X}", vram.read_byte(0x1FFF));

    // PPU framebuffer 測試
    let fb = ppu.get_framebuffer();
    println!("PPU framebuffer size: {}", fb.len());

    // ROM/VRAM/PPU 互動測試
    // 假設 MMU 有 work_ram 作為 ROM 資料來源
    let rom_data: [u8; 16] = [
        0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF,
        0x00,
    ];
    // 自動化 ROM→VRAM→PPU→framebuffer 全流程測試
    // Step 1: ROM→VRAM
    for (i, &val) in rom_data.iter().enumerate() {
        vram.write_byte(i, val);
    }
    let vram_check: Vec<u8> = (0..16).map(|i| vram.read_byte(i)).collect();
    assert_eq!(vram_check, rom_data, "VRAM 寫入與 ROM 資料不符");
    println!("[自動化] VRAM 寫入驗證通過");

    // Step 2: VRAM→PPU
    ppu.vram[..16].copy_from_slice(&vram.data[..16]);
    let ppu_vram_check = &ppu.vram[..16];
    assert_eq!(ppu_vram_check, &rom_data[..], "PPU VRAM 與 ROM 資料不符");
    println!("[自動化] PPU VRAM 同步驗證通過");

    // Step 3: PPU→framebuffer
    ppu.tick();
    let fb = ppu.get_framebuffer();
    // 這裡假設 framebuffer[0..16] 會根據 VRAM 產生像素（如 tile/palette 渲染）
    // 若目前 PPU 邏輯僅複製資料，則直接比對
    let fb_check = &fb[..16];
    // 可根據 PPU 實作調整驗證條件
    println!("[自動化] framebuffer[0..16]: {:?}", fb_check);
    // 若需 assert，請根據 PPU 實際渲染邏輯補齊
    // assert_eq!(fb_check, &rom_data[..], "framebuffer 與 ROM 資料不符");
    println!("[自動化] ROM→VRAM→PPU→framebuffer 全流程測試結束");

    // GameBoy 記憶體互動範例
    // 1. MMU work_ram 寫入/讀取
    {
        let mut mmu_ref = mmu.borrow_mut();
        mmu_ref.work_ram[0] = 0x42;
        let ram_val = mmu_ref.work_ram[0];
        println!("MMU work_ram[0] = {:#X}", ram_val);

        // 2. MMU→VRAM 資料流
        vram.write_byte(0, mmu_ref.work_ram[0]);
        println!("VRAM[0x0000] after MMU copy = {:#X}", vram.read_byte(0));
    }
    // 3. VRAM→PPU 渲染
    ppu.vram[0] = vram.read_byte(0);
    ppu.vram[1] = 0; // hi byte
    ppu.tick();
    println!(
        "PPU framebuffer[0] after MMU→VRAM→PPU = {}",
        ppu.get_framebuffer()[0]
    );

    // 4. 直接用 CPU 操作 MMU
    {
        let mut cpu_mmu = cpu.mmu().borrow_mut();
        cpu_mmu.work_ram[1] = 0x99;
    }
    let cpu_ram_val = cpu.mmu().borrow().work_ram[1];
    println!("CPU 讀取 MMU work_ram[1] = {:#X}", cpu_ram_val);
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::core::cpu::flags::Flag;

    fn setup_cpu() -> CPU {
        let mmu = Rc::new(RefCell::new(MMU::new()));
        CPU::new(mmu)
    }

    #[test]
    fn test_bit_7_b_set() {
        let mut cpu = setup_cpu();
        cpu.registers_mut().b = 0b1000_0000;
        let _ = crate::core::cpu::bit::dispatch(&mut cpu, 0x78);
        assert_eq!(
            cpu.registers().get_flag(Flag::Z),
            false,
            "BIT 7,B 應為 false"
        );
    }

    #[test]
    fn test_bit_7_b_clear() {
        let mut cpu = setup_cpu();
        cpu.registers_mut().b = 0b0000_0000;
        let _ = crate::core::cpu::bit::dispatch(&mut cpu, 0x78);
        assert_eq!(cpu.registers().get_flag(Flag::Z), true, "BIT 7,B 應為 true");
    }

    #[test]
    fn test_ld_b_n() {
        let mut cpu = setup_cpu();
        cpu.registers_mut().b = 0;
        cpu.registers_mut().pc = 0xC000;
        // 模擬記憶體中 work_ram[0] 對應 0xC000
        cpu.mmu().borrow_mut().work_ram[0] = 0x42;
        let _ = crate::core::cpu::load::dispatch(&mut cpu, 0x06);
        assert_eq!(cpu.registers().b, 0x42, "LD B,n 應正確載入 0x42");
    }

    #[test]
    fn test_or_a_b() {
        let mut cpu = setup_cpu();
        cpu.registers_mut().a = 0b0000_1100;
        cpu.registers_mut().b = 0b0000_1010;
        let a_before = cpu.registers().a;
        let b_before = cpu.registers().b;
        let _ = crate::core::cpu::logic::dispatch(&mut cpu, 0xB0);
        let a_after = cpu.registers().a;
        println!(
            "OR A,B 測試: A_before={:#b}, B_before={:#b}, A_after={:#b}",
            a_before, b_before, a_after
        );
        assert_eq!(a_after, 0b0000_1110, "OR A,B 應正確運算");
    }
}
