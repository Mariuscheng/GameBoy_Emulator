extern crate sdl3;

mod core;
mod interface;
mod utils;

use core::mmu::MMU;
use core::ppu::PPU;
use interface::sdl3_display::Display;
use interface::video::VideoInterface;
use sdl3::{
    event::Event,
    keyboard::Scancode,
    log::{Category, Priority},
};
use std::{cell::RefCell, rc::Rc, time::Duration};

pub fn main() -> Result<(), String> {
    // 初始化 SDL 日誌
    sdl3::log::set_log_priority(Category::Audio, Priority::Info);
    sdl3::log::set_log_priority(Category::Video, Priority::Info);

    // 初始化 SDL
    let sdl_context = sdl3::init().map_err(|e| e.to_string())?;
    // TODO: 稍後實現音頻系統
    let _ = sdl_context.audio().map_err(|e| e.to_string())?;

    // 初始化顯示
    let display = Display::new(&sdl_context).map_err(|e| e.to_string())?;

    // 初始化 MMU
    let mmu = Rc::new(RefCell::new(MMU::new()));

    // 初始化PPU寄存器
    {
        let mut mmu = mmu.borrow_mut();

        // LCDC (0xFF40) - LCD控制
        // Bit 7: LCD啟用
        // Bit 6: 視窗區塊映射選擇
        // Bit 5: 視窗啟用
        // Bit 4: BG/視窗圖塊數據選擇
        // Bit 3: BG區塊映射選擇
        // Bit 2: 精靈大小 (0=8x8, 1=8x16)
        // Bit 1: 精靈啟用
        // Bit 0: 背景啟用
        mmu.write_byte(0xFF40, 0x91).map_err(|e| e.to_string())?;

        // SCY (0xFF42) - 背景Y捲動
        mmu.write_byte(0xFF42, 0x00).map_err(|e| e.to_string())?;

        // SCX (0xFF43) - 背景X捲動
        mmu.write_byte(0xFF43, 0x00).map_err(|e| e.to_string())?;

        // BGP (0xFF47) - 背景調色板
        // 每2位代表一個顏色:
        // 00=最亮 01=亮 10=暗 11=最暗
        mmu.write_byte(0xFF47, 0xE4).map_err(|e| e.to_string())?; // 11 10 01 00

        // OBP0 (0xFF48) - 精靈調色板0
        mmu.write_byte(0xFF48, 0xE4).map_err(|e| e.to_string())?;

        // OBP1 (0xFF49) - 精靈調色板1
        mmu.write_byte(0xFF49, 0xE4).map_err(|e| e.to_string())?;

        println!("PPU寄存器初始化完成");
    }

    // 初始化 PPU
    let mut ppu = PPU::new(Rc::clone(&mmu), Box::new(display));

    // 初始化 Timer
    let mut timer = core::timer::Timer::new();

    // 讀取ROM文件
    let rom_path = "roms/rom.gb";
    println!("正在載入ROM文件: {}", rom_path);
    let rom_data = {
        // 先嘗試相對路徑
        let result = std::fs::read(rom_path);
        match result {
            Ok(data) => {
                println!("成功讀取ROM文件，大小: {} bytes", data.len());
                println!("ROM文件前16字節:");
                for i in 0..16 {
                    print!("{:02X} ", data[i]);
                }
                println!("\n");
                data
            }
            Err(e) => {
                println!("相對路徑讀取失敗: {}", e);
                println!("嘗試從絕對路徑讀取...");
                let absolute_path = std::env::current_dir()
                    .map_err(|e| e.to_string())?
                    .join(rom_path);
                println!("嘗試讀取: {}", absolute_path.display());
                let data = std::fs::read(&absolute_path)
                    .map_err(|e| format!("無法讀取ROM文件 {}: {}", absolute_path.display(), e))?;
                println!("從絕對路徑成功讀取，大小: {} bytes", data.len());
                println!("ROM文件前16字節:");
                for i in 0..16 {
                    print!("{:02X} ", data[i]);
                }
                println!("\n");
                data
            }
        }
    };

    // 顯示ROM信息
    {
        println!("ROM標頭信息:");
        let title = String::from_utf8_lossy(&rom_data[0x134..0x144]);
        let title_trimmed = title.trim_matches(char::from(0));
        println!("標題: \"{}\"", title_trimmed);

        let manufacturer = String::from_utf8_lossy(&rom_data[0x13F..0x143]);
        let manufacturer_trimmed = manufacturer.trim_matches(char::from(0));
        println!("製造商代碼: \"{}\"", manufacturer_trimmed);

        println!("CGB標誌: 0x{:02X}", rom_data[0x143]);

        let licensee = String::from_utf8_lossy(&rom_data[0x144..0x146]);
        let licensee_trimmed = licensee.trim_matches(char::from(0));
        println!("新授權碼: \"{}\"", licensee_trimmed);

        println!("SGB標誌: 0x{:02X}", rom_data[0x146]);
        println!("卡帶類型: 0x{:02X}", rom_data[0x147]);
        println!(
            "ROM大小: 0x{:02X} ({} KB)",
            rom_data[0x148],
            32 << rom_data[0x148]
        );
        println!("RAM大小: 0x{:02X}", rom_data[0x149]);
        println!("目標市場: 0x{:02X}", rom_data[0x14A]);

        println!("\nROM數據示例:");
        println!("不同區域的ROM數據:");

        // 顯示開始區域
        println!("1. ROM起始區域 (0x0000-0x00FF):");
        for i in 0..32 {
            print!("{:02X} ", rom_data[i]);
            if (i + 1) % 16 == 0 {
                println!();
            }
        }
        println!();

        // 顯示標頭區域附近
        println!("2. ROM標頭區域 (0x0130-0x014F):");
        for i in 0x130..0x150 {
            print!("{:02X} ", rom_data[i]);
            if (i + 1) % 16 == 0 {
                println!();
            }
        }
        println!();

        // 顯示圖塊數據區域
        println!("3. 圖塊數據區域示例 (0x8000-0x8020):");
        if rom_data.len() >= 0x8020 {
            for i in 0x8000..0x8020 {
                print!("{:02X} ", rom_data[i]);
                if (i + 1) % 16 == 0 {
                    println!();
                }
            }
        } else {
            println!("ROM 大小不足以包含此區域");
        }
        println!();
    }

    // 載入ROM到MMU
    println!("正在載入ROM到MMU...");
    mmu.borrow_mut()
        .load_rom(rom_data)
        .map_err(|e| e.to_string())?;
    println!("ROM載入完成");

    // 顯示 VRAM 中的一些數據
    println!("VRAM 數據驗證:");
    println!("1. 圖塊數據區域 (0x8000-0x87FF):");
    for i in 0..32 {
        let value = mmu
            .borrow()
            .read_byte(0x8000_u16.wrapping_add(i))
            .map_err(|e| e.to_string())?;
        print!("{:02X} ", value);
        if (i + 1) % 16 == 0 {
            println!();
        }
    }

    println!("\n2. 背景圖塊地圖 1 (0x9800-0x9BFF) 的前32字節:");
    for i in 0..32 {
        let value = mmu
            .borrow()
            .read_byte(0x9800_u16.wrapping_add(i))
            .map_err(|e| e.to_string())?;
        print!("{:02X} ", value);
        if (i + 1) % 16 == 0 {
            println!();
        }
    }

    println!("\n3. 背景圖塊地圖 2 (0x9C00-0x9FFF) 的前32字節:");
    for i in 0..32 {
        let value = mmu
            .borrow()
            .read_byte(0x9C00_u16.wrapping_add(i))
            .map_err(|e| e.to_string())?;
        print!("{:02X} ", value);
        if (i + 1) % 16 == 0 {
            println!();
        }
    }

    println!("\n4. 檢查VRAM寫入測試:");
    {
        let mut mmu = mmu.borrow_mut();
        // 寫入測試數據
        mmu.write_byte(0x8000, 0xAA).map_err(|e| e.to_string())?;
        mmu.write_byte(0x8001, 0x55).map_err(|e| e.to_string())?;

        // 讀取並驗證
        let test_value1 = mmu.read_byte(0x8000).map_err(|e| e.to_string())?;
        let test_value2 = mmu.read_byte(0x8001).map_err(|e| e.to_string())?;
        println!("寫入測試: 0x8000 = 0x{:02X} (應為 0xAA)", test_value1);
        println!("寫入測試: 0x8001 = 0x{:02X} (應為 0x55)", test_value2);
    }
    println!("\n");

    let mut frame_count = 0;
    let mut last_time = std::time::Instant::now();
    let frame_rate = 60.0;

    // 在主循環之前創建 event_pump
    let mut event_pump = sdl_context.event_pump().map_err(|e| e.to_string())?;

    'running: loop {
        let frame_start = std::time::Instant::now();

        // 執行一個完整的 CPU 幀
        let mut cycles_this_frame = 0;
        let target_cycles = 70224; // GameBoy 的每幀週期數 (4.194304MHz / 60fps ≈ 70224)

        while cycles_this_frame < target_cycles {
            // 執行一個 CPU 週期
            let cycles = 4; // 暫時固定為 4 個週期
            cycles_this_frame += cycles;

            // 更新 PPU
            ppu.update(cycles).map_err(|e| e.to_string())?;

            // 更新 Timer
            timer.update(cycles).map_err(|e| e.to_string())?;
        }

        // 控制幀率
        let frame_time = frame_start.elapsed();
        let target_frame_time = Duration::from_secs_f64(1.0 / frame_rate);
        if frame_time < target_frame_time {
            std::thread::sleep(target_frame_time - frame_time);
        }

        frame_count += 1;
        if frame_count == 60 {
            // 每60幀顯示一次統計資訊
            let current_time = std::time::Instant::now();
            let elapsed = current_time.duration_since(last_time);
            let fps = frame_count as f64 / elapsed.as_secs_f64();
            let frame_time = elapsed.as_micros() / frame_count as u128;

            println!("FPS: {:.2}, 平均幀時間: {}µs", fps, frame_time);

            frame_count = 0;
            last_time = current_time;
        }

        // SDL3 事件處理
        while let Some(event) = event_pump.poll_event() {
            match event {
                Event::Quit { .. } => break 'running,
                Event::KeyDown {
                    scancode: Some(Scancode::Escape),
                    ..
                } => break 'running,
                Event::JoyButtonDown { button_idx, .. } => {
                    println!("搖桿按鈕 {} 被按下", button_idx);
                }
                Event::JoyAxisMotion {
                    axis_idx, value, ..
                } => {
                    println!("搖桿軸 {} 移動到 {}", axis_idx, value);
                }
                _ => {}
            }
        }
    }

    Ok(())
}
