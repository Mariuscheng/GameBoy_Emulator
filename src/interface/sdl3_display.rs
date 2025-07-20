extern crate sdl3;

use crate::core::cpu::cpu::CPU;
use crate::core::mmu::MMU;
use crate::core::ppu::display::Display;
use crate::core::ppu::ppu::PPU;
use crate::interface::audio::AudioInterface;
use sdl3::event::Event;
use sdl3::keyboard::Keycode;
use sdl3::pixels::Color;
use std::fs::File;
use std::io::Read;
// use sdl3::rect::Rect; // 不再需要
use sdl3::render::Canvas;
use sdl3::video::Window;
use std::time::Duration;

pub fn init_sdl() {
    let sdl_context = sdl3::init().unwrap();
    let video_subsystem = sdl_context.video().unwrap();

    let window = video_subsystem
        .window("rust-sdl3 demo", 800, 600)
        .position_centered()
        .build()
        .unwrap();

    let mut canvas = window.into_canvas();
    canvas.clear();
    canvas.present();
    let mut event_pump = sdl_context.event_pump().unwrap();

    // 載入 ROM
    let mut rom_data = Vec::new();
    let mut rom_file = File::open("roms/rom.gb").expect("找不到 ROM 檔案");
    rom_file.read_to_end(&mut rom_data).expect("讀取 ROM 失敗");
    // Print ROM 開頭 32 bytes（opcode）
    println!("[ROM Debug] 前 32 bytes:");
    for i in 0..32 {
        if i < rom_data.len() {
            print!("{:02X} ", rom_data[i]);
        }
    }
    println!("");

    // 初始化 MMU、CPU、Display
    let mut mmu = MMU::new();
    // 確保 mmu.cartridge_rom 長度正確
    if rom_data.len() > 0 {
        mmu.cartridge_rom = vec![0; rom_data.len()];
        for (i, &b) in rom_data.iter().enumerate() {
            mmu.cartridge_rom[i] = b;
        }
    }
    let mmu_rc = std::rc::Rc::new(std::cell::RefCell::new(mmu));
    let mut ppu = PPU::new(mmu_rc.clone());
    let mut cpu = CPU::new(mmu_rc.clone());
    // GameBoy 標準啟動點 PC = 0x0100
    cpu.registers.pc = 0x0100;

    'running: loop {
        AudioInterface::new().unwrap().start().unwrap();
        // 執行 CPU 指令，推進遊戲狀態
        for _ in 0..100000 {
            // 取得下一個 opcode 並執行
            let pc = cpu.registers.pc;
            if (pc as usize) >= mmu_rc.borrow().cartridge_rom.len() {
                break;
            }
            let opcode = mmu_rc.borrow().cartridge_rom[pc as usize];
            if pc % 1000 == 0 {
                let video_ram = &mmu_rc.borrow().video_ram;
                println!(
                    "[CPU Debug] PC: {:04X}, opcode: {:02X}, video_ram[0..16]: {:?}",
                    pc,
                    opcode,
                    &video_ram[0..16]
                );
            }
            crate::core::cpu::cb::dispatch(&mut cpu, opcode).unwrap_or_default();
        }
        // 執行 PPU 週期，更新畫面
        for _ in 0..1000 {
            // 執行 PPU 週期
            ppu.tick();
        }
        // 取得 PPU 畫面緩衝區
        canvas.clear();
        let fb = ppu.get_framebuffer();
        // 防呆：framebuffer 長度不足時跳過繪製
        let mut rgb_fb = vec![0xFFFFFFFFu32; 160 * 144];
        if fb.len() < 160 * 144 {
            println!("[PPU Debug] framebuffer 長度不足: {}，以全白填充", fb.len());
            // rgb_fb 已預設全白，直接繪製
        } else {
            for (i, &pix) in fb.iter().enumerate().take(160 * 144) {
                let color = match pix {
                    0 => 0xFFFFFFFF, // 白
                    1 => 0xFFAAAAAA, // 淺灰
                    2 => 0xFF555555, // 深灰
                    3 => 0xFF000000, // 黑
                    _ => 0xFFFFFFFF,
                };
                rgb_fb[i] = color;
            }
        }
        draw_framebuffer(&mut canvas, &rgb_fb);
        for event in event_pump.poll_iter() {
            match event {
                Event::Quit { .. }
                | Event::KeyDown {
                    keycode: Some(Keycode::Escape),
                    ..
                } => break 'running,
                _ => {}
            }
        }
        canvas.present();
        ::std::thread::sleep(Duration::new(0, 1_000_000_000u32 / 60));
    }

    /// 將 GameBoy framebuffer 畫到 SDL3 canvas
    fn draw_framebuffer(canvas: &mut Canvas<Window>, framebuffer: &[u32]) {
        // 防呆：framebuffer 長度不足時直接回傳不畫
        if framebuffer.len() < 160 * 144 {
            println!(
                "[SDL3 Debug] draw_framebuffer 跳過，framebuffer 長度不足: {}",
                framebuffer.len()
            );
            return;
        }
        // GameBoy 畫面 160x144
        for y in 0..144 {
            for x in 0..160 {
                let idx = y * 160 + x;
                let color = framebuffer[idx];
                let r = ((color >> 16) & 0xFF) as u8;
                let g = ((color >> 8) & 0xFF) as u8;
                let b = (color & 0xFF) as u8;
                canvas.set_draw_color(Color::RGB(r, g, b));
                let _ = canvas.draw_point((x as i32, y as i32));
            }
        }
    }
}
