#[allow(non_snake_case)]
mod GB;
mod interface;

use std::env;
use std::fs;
use std::io::Read;
use std::path::Path;

use interface::sdl3_display::SdlDisplay;
// use sdl3::keyboard::Scancode; // 熱鍵暫停使用
// use std::time::{Duration, Instant};

fn main() {
    // 初始化 CPU 與載入 ROM（可選）
    let mut cpu = GB::CPU::CPU::new();
    // 先看命令列參數是否有指定 ROM 路徑；否則落回候選清單
    let mut rom_loaded = false;
    if let Some(arg1) = env::args().nth(1) {
        let p = Path::new(&arg1);
        if p.exists() {
            if let Ok(mut f) = fs::File::open(p) {
                let mut buf = Vec::new();
                let _ = f.read_to_end(&mut buf);
                // 將整個 ROM 載入 Bus（支援 MBC）
                cpu.memory.load_rom(buf);
                println!("Loaded ROM (arg): {}", p.display());
                rom_loaded = true;
            }
        } else {
            eprintln!("ROM path not found: {} (fallback to defaults)", p.display());
        }
    }
    if !rom_loaded {
        // 嘗試從常見位置載入（簡化：直接寫入 0x0000..）
        // 將預設載入順序改為優先工作資料夾的 roms/rom.gb（通常是使用者目標 ROM）
        let candidates = [
            Path::new("roms/rom.gb"),
            Path::new("pokemon_yellow.gb"),
            Path::new("Tetris (Japan) (En).gb"),
            Path::new("dmg-acid2.gb"),
        ];
        for p in candidates.iter() {
            if p.exists() {
                if let Ok(mut f) = fs::File::open(p) {
                    let mut buf = Vec::new();
                    let _ = f.read_to_end(&mut buf);
                    cpu.memory.load_rom(buf);
                    println!("Loaded ROM: {}", p.display());
                    rom_loaded = true;
                    break;
                }
            }
        }
        if !rom_loaded {
            eprintln!("No ROM found. Provide a path: cargo run -- <path-to-rom.gb>");
        }
    }

    // 確認 ROM 映射：印出 0x0100 起始的 16 bytes（一次）
    {
        let mut bytes = [0u8; 16];
        for i in 0..16u16 {
            bytes[i as usize] = cpu.memory.read(0x0100 + i);
        }
        println!("ROM[0100..0110]: {:02X?}", bytes);
    }

    // 跳過 BIOS 的常見初始化（Pan Docs post-BIOS defaults）
    // 寄存器
    cpu.registers.set_a(0x01);
    cpu.registers.set_f(0xB0);
    cpu.registers.set_b(0x00);
    cpu.registers.set_c(0x13);
    cpu.registers.set_d(0x00);
    cpu.registers.set_e(0xD8);
    cpu.registers.set_h(0x01);
    cpu.registers.set_l(0x4D);
    cpu.registers.set_sp(0xFFFE);
    cpu.registers.set_pc(0x0100);
    // IO 預設
    cpu.memory.write(0xFF05, 0x00); // TIMA
    cpu.memory.write(0xFF06, 0x00); // TMA
    cpu.memory.write(0xFF07, 0x00); // TAC
    cpu.memory.write(0xFF10, 0x80);
    cpu.memory.write(0xFF11, 0xBF);
    cpu.memory.write(0xFF12, 0xF3);
    cpu.memory.write(0xFF14, 0xBF);
    cpu.memory.write(0xFF16, 0x3F);
    cpu.memory.write(0xFF17, 0x00);
    cpu.memory.write(0xFF19, 0xBF);
    cpu.memory.write(0xFF1A, 0x7F);
    cpu.memory.write(0xFF1B, 0xFF);
    cpu.memory.write(0xFF1C, 0x9F);
    cpu.memory.write(0xFF1E, 0xBF);
    cpu.memory.write(0xFF20, 0xFF);
    cpu.memory.write(0xFF21, 0x00);
    cpu.memory.write(0xFF22, 0x00);
    cpu.memory.write(0xFF23, 0xBF);
    cpu.memory.write(0xFF24, 0x77);
    cpu.memory.write(0xFF25, 0xF3);
    cpu.memory.write(0xFF26, 0xF1); // (DMG) 0xF1, (SGB) 0xF0
    // 比照 BIOS 的常見 post-BIOS 預設：直接開啟 LCD（0x91）
    // 多數商用 ROM 預設假設 BIOS 已開啟 LCD；若需要關閉再自行關閉
    cpu.memory.write(0xFF40, 0x91);
    cpu.memory.write(0xFF42, 0x00); // SCY
    cpu.memory.write(0xFF43, 0x00); // SCX
    cpu.memory.write(0xFF45, 0x00); // LYC
    cpu.memory.write(0xFF47, 0xFC); // BGP
    cpu.memory.write(0xFF48, 0xFF); // OBP0（比照 BIOS 預設）
    cpu.memory.write(0xFF49, 0xFF); // OBP1（比照 BIOS 預設）
    cpu.memory.write(0xFF4A, 0x00); // WY
    cpu.memory.write(0xFF4B, 0x00); // WX
    cpu.memory.write(0xFFFF, 0x00); // IE

    // 建立 SDL 視窗
    let scale = 3u32;
    let mut display = SdlDisplay::new("Rust GB", scale).expect("SDL init failed");

    // 以 PPU VBlank 作為畫面同步點，避免撕裂
    let mut quit = false;
    let mut frame_counter: u64 = 0;
    let mut instr_counter: u64 = 0; // 心跳：每 50 萬指令印狀態
    let mut prev_in_vblank = false;
    // let mut lcdc80_frames: u64 = 0; // 停用：不再自動協助修改 LCDC
    // 熱鍵暫停：不追蹤任何 F-key 邊緣狀態與測試覆蓋

    while !quit {
        // 以指令為粒度執行
        let _taken = cpu.execute_next();
        instr_counter = instr_counter.wrapping_add(1);

        // 若一直沒有進入 VBlank，也定期輸出 LCDC/LY 與 VRAM/TileMap 的非零統計，便於除錯
        if instr_counter % 500_000 == 0 {
            let mut vram_nonzero = 0usize;
            for addr in 0x8000u16..=0x9FFFu16 {
                if cpu.memory.read(addr) != 0 {
                    vram_nonzero += 1;
                }
            }
            let mut bgmap_nonzero = 0usize;
            for addr in 0x9800u16..=0x9BFFu16 {
                if cpu.memory.read(addr) != 0 {
                    bgmap_nonzero += 1;
                }
            }
            let lcdc = cpu.memory.read(0xFF40);
            let ly = cpu.memory.read(0xFF44);
            let pc = cpu.registers.get_pc();
            let opcode = cpu.memory.read(pc);
            let ie = cpu.memory.read(0xFFFF);
            let iflag = cpu.memory.read(0xFF0F);
            let ime = cpu.ime;
            let halted = cpu.halted;
            println!(
                "Heartbeat ({}k instr) | PC={:04X} OP={:02X} | LY={} | LCDC={:02X} | IE={:02X} IF={:02X} IME={} HALT={} | VRAM!=0: {} / 8192 | BGMap!=0: {} / 1024",
                instr_counter / 1000,
                pc,
                opcode,
                ly,
                lcdc,
                ie,
                iflag,
                ime,
                halted,
                vram_nonzero,
                bgmap_nonzero
            );
        }

        // 輸入更新（joypad），並檢查是否退出
        quit = display.pump_events_and_update_joypad(|p1| {
            cpu.memory.set_joypad_state(p1);
        });

        // 熱鍵功能已停用：不再改動 LCDC/VRAM 或注入任何測試畫面

        // 在 VBlank 上緣執行 blit，避免撕裂與閃爍
        let in_vblank = cpu.memory.read(0xFF44) >= 144;
        if in_vblank && !prev_in_vblank {
            frame_counter += 1;
            let mut shades = vec![0u8; 160 * 144];
            for y in 0..144usize {
                for x in 0..160usize {
                    shades[y * 160 + x] = cpu.memory.get_fb_pixel(x, y);
                }
            }
            // 不再在 LCD 關閉時強制清白，保留上一幀內容以符合多數真機觀感
            let _ = display.blit_framebuffer(&shades);

            // 每 30 幀偵測一次 VRAM/Tilemap 是否已有內容，方便除錯
            if frame_counter % 30 == 0 {
                let mut vram_nonzero = 0usize;
                for addr in 0x8000u16..=0x9FFFu16 {
                    if cpu.memory.read(addr) != 0 {
                        vram_nonzero += 1;
                    }
                }
                let mut bgmap_nonzero = 0usize;
                for addr in 0x9800u16..=0x9BFFu16 {
                    if cpu.memory.read(addr) != 0 {
                        bgmap_nonzero += 1;
                    }
                }
                println!(
                    "Frame {} | VRAM!=0: {} / 8192 | BGMap!=0: {} / 1024 | LCDC={:02X}",
                    frame_counter,
                    vram_nonzero,
                    bgmap_nonzero,
                    cpu.memory.read(0xFF40)
                );
            }
            // 停用自動協助：不會自動更改 LCDC（完全交給 ROM 控制）

            // 輕量節流，避免滿速旋轉（SDL 會自行限頻時也可視情況移除）
            std::thread::sleep(std::time::Duration::from_millis(1));
        }
        prev_in_vblank = in_vblank;
    }
}
