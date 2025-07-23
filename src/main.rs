use crate::core::cpu::registers::Registers;
mod core;
mod interface;
//mod utils;
// 已完全棄用 interface::sdl3_display，移除未用 imports
use std::fs::File;
use std::io::Read;
//use utils::emulator_api::Emulator;
// use utils::mmu_api::MMUApi; // Removed because MMUApi does not exist in mmu_api
//use utils::ppu_api::PPU;

fn main() {
    // 整合 CPU、PPU、APU
    use crate::core::apu::APU;
    use crate::core::cpu::cpu::CPU;
    use crate::core::mmu::mmu::MMU;
    use crate::core::ppu::ppu::PPU;

    // 讀取 ROM 檔案（命令列參數或預設路徑）
    let args: Vec<String> = std::env::args().collect();
    let rom_path = if args.len() > 1 { &args[1] } else { "rom.gb" };
    let mut rom_file = File::open(rom_path).expect("找不到 ROM 檔案，請確認路徑！");
    let mut rom_buffer = Vec::new();
    rom_file
        .read_to_end(&mut rom_buffer)
        .expect("ROM 讀取失敗！");
    // 檢查 ROM 大小
    if rom_buffer.len() < 0x8000 {
        panic!("ROM 檔案過小，請確認格式！");
    }
    // 初始化 MMU，傳入 ROM buffer
    let mut mmu = MMU::from_buffer(&rom_buffer);
    // SDL3 官方架構：直接在 main.rs 建立 window/canvas/event_pump
    let sdl_context = sdl3::init().unwrap();
    let video_subsystem = sdl_context.video().unwrap();
    let window = video_subsystem
        .window("GameBoy模擬器", 320, 288)
        .position_centered()
        .build()
        .unwrap();
    let mut canvas = window.into_canvas();
    let mut event_pump = sdl_context.event_pump().unwrap();
    // 初始化 CPU，不再注入 sdl_backend
    let mut cpu = CPU {
        ime: false,
        halted: false,
        stopped: false,
        registers: Registers::default(),
        mmu: &mut mmu as *mut MMU,
        sdl_backend: None,
        sdl_event_pump: None,
    };
    // 初始化 PPU，先取得 vram 指標再傳入
    let vram_ptr = &mut mmu.vram as *mut crate::core::ppu::vram::VRAM;
    let mut ppu = PPU::new(&mut mmu, unsafe { &mut *vram_ptr });
    // 初始化 PPU
    // framebuffer 已無用，移除
    // 移除重複的 CPU 初始化，僅保留上方正確初始化
    // 其他 PPU/顯示/時序等欄位請獨立初始化
    // 初始化 APU
    let apu = APU::new();

    // 主要執行循環（60FPS，時序同步）
    let target_fps = 60;
    let frame_duration = std::time::Duration::from_micros(1_000_000 / target_fps);
    let mut i = 0;
    'running: loop {
        i = (i + 1) % 255;
        let frame_start = std::time::Instant::now();
        // 每 frame 只執行一次 step，避免死循環
        cpu.step();
        ppu.step();
        apu.mix(44100, 16);
        // SDL3 event loop（官方範例架構）
        for event in event_pump.poll_iter() {
            match event {
                sdl3::event::Event::Quit { .. }
                | sdl3::event::Event::KeyDown {
                    keycode: Some(sdl3::keyboard::Keycode::Escape),
                    ..
                } => {
                    break 'running;
                }
                _ => {}
            }
        }
        // 回退到逐點繪製（2x 放大）
        canvas.set_draw_color(sdl3::pixels::Color::RGB(255, 255, 255));
        canvas.clear();
        let colors = ppu
            .get_framebuffer_indices()
            .iter()
            .map(|&idx| {
                match idx {
                    0 => sdl3::pixels::Color::RGB(224, 248, 208), // 白
                    1 => sdl3::pixels::Color::RGB(136, 192, 112), // 淺灰
                    2 => sdl3::pixels::Color::RGB(52, 104, 86),   // 深灰
                    3 => sdl3::pixels::Color::RGB(8, 24, 32),     // 黑
                    _ => sdl3::pixels::Color::RGB(255, 0, 0),     // 錯誤值顯示紅
                }
            })
            .collect::<Vec<_>>();
        let width = 160;
        let height = 144;
        for y in 0..height {
            for x in 0..width {
                let idx = y * width + x;
                if idx < colors.len() {
                    canvas.set_draw_color(colors[idx]);
                    let _ = canvas.draw_point((x as i32 * 2, y as i32 * 2));
                }
            }
        }
        canvas.present();
        let elapsed = frame_start.elapsed();
        if elapsed < frame_duration {
            std::thread::sleep(frame_duration - elapsed);
        }
    }
}
