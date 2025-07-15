use chrono;
use std::fs::{self, File};
use std::io::Read;
use winit::application::ApplicationHandler;
use winit::event::{ElementState, WindowEvent};
use winit::event_loop::ActiveEventLoop;
use winit::keyboard::{Key, NamedKey};
use winit::window::{Window, WindowAttributes};

use gameboy_emulator::interface::input::joypad::Joypad;
use gameboy_emulator::{
    error::{Error, HardwareError, Result},
    interface::{
        audio::AudioInterface, input::joypad::GameBoyKey, input::simple_joypad::SimpleJoypad,
        video::PixelsDisplay,
    },
    GameBoy,
};
use winit::event_loop::EventLoop;

#[derive(Debug)]
struct DummyAudio;

impl AudioInterface for DummyAudio {
    fn push_sample(&mut self, _sample: f32) {}
    fn start(&mut self) {}
    fn stop(&mut self) {}
}

struct EmulatorApp {
    rom_data: Vec<u8>,
    gameboy: Option<GameBoy>,
    joypad: SimpleJoypad,
    window: Option<&'static Window>,
}

impl EmulatorApp {
    fn new(rom_data: Vec<u8>) -> Self {
        EmulatorApp {
            rom_data,
            gameboy: None,
            joypad: SimpleJoypad::new(),
            window: None,
        }
    }
}

impl ApplicationHandler for EmulatorApp {
    fn resumed(&mut self, event_loop: &ActiveEventLoop) {
        // 建立視窗
        let attrs = WindowAttributes::default()
            .with_title("GameBoy Emulator")
            .with_inner_size(winit::dpi::LogicalSize::new(160.0 * 2.0, 144.0 * 2.0));
        let window = Box::leak(Box::new(
            event_loop
                .create_window(attrs)
                .expect("Failed to create window"),
        ));
        self.window = Some(window);

        // 建立 PixelsDisplay
        let video = PixelsDisplay::new(window)
            .map_err(|e| Error::Hardware(HardwareError::Display(e.to_string())))
            .expect("Failed to create PixelsDisplay");
        let mut gameboy = GameBoy::new(Box::new(video), Some(Box::new(DummyAudio)))
            .expect("Failed to create GameBoy");
        gameboy
            .load_rom(self.rom_data.clone())
            .expect("Failed to load ROM");
        println!("ROM loaded successfully");
        self.gameboy = Some(gameboy);
    }

    fn window_event(
        &mut self,
        event_loop: &ActiveEventLoop,
        window_id: winit::window::WindowId,
        event: WindowEvent,
    ) {
        if let Some(window) = self.window {
            if window.id() != window_id {
                return;
            }
        }
        match event {
            WindowEvent::CloseRequested => {
                println!("Window closed");
                event_loop.exit();
            }
            WindowEvent::KeyboardInput { event, .. } => {
                let winit::event::KeyEvent {
                    state,
                    logical_key: key,
                    ..
                } = event;
                match (key, state) {
                    (Key::Character(ref s), ElementState::Pressed) if s == "a" => {
                        self.joypad.press_key(GameBoyKey::A)
                    }
                    (Key::Character(ref s), ElementState::Released) if s == "a" => {
                        self.joypad.release_key(GameBoyKey::A)
                    }
                    (Key::Character(ref s), ElementState::Pressed) if s == "b" => {
                        self.joypad.press_key(GameBoyKey::B)
                    }
                    (Key::Character(ref s), ElementState::Released) if s == "b" => {
                        self.joypad.release_key(GameBoyKey::B)
                    }
                    (Key::Named(NamedKey::ArrowLeft), ElementState::Pressed) => {
                        self.joypad.press_key(GameBoyKey::Left)
                    }
                    (Key::Named(NamedKey::ArrowLeft), ElementState::Released) => {
                        self.joypad.release_key(GameBoyKey::Left)
                    }
                    (Key::Named(NamedKey::ArrowRight), ElementState::Pressed) => {
                        self.joypad.press_key(GameBoyKey::Right)
                    }
                    (Key::Named(NamedKey::ArrowRight), ElementState::Released) => {
                        self.joypad.release_key(GameBoyKey::Right)
                    }
                    (Key::Named(NamedKey::ArrowUp), ElementState::Pressed) => {
                        self.joypad.press_key(GameBoyKey::Up)
                    }
                    (Key::Named(NamedKey::ArrowUp), ElementState::Released) => {
                        self.joypad.release_key(GameBoyKey::Up)
                    }
                    (Key::Named(NamedKey::ArrowDown), ElementState::Pressed) => {
                        self.joypad.press_key(GameBoyKey::Down)
                    }
                    (Key::Named(NamedKey::ArrowDown), ElementState::Released) => {
                        self.joypad.release_key(GameBoyKey::Down)
                    }
                    (Key::Named(NamedKey::Enter), ElementState::Pressed) => {
                        self.joypad.press_key(GameBoyKey::Start)
                    }
                    (Key::Named(NamedKey::Enter), ElementState::Released) => {
                        self.joypad.release_key(GameBoyKey::Start)
                    }
                    (Key::Named(NamedKey::Space), ElementState::Pressed) => {
                        self.joypad.press_key(GameBoyKey::Select)
                    }
                    (Key::Named(NamedKey::Space), ElementState::Released) => {
                        self.joypad.release_key(GameBoyKey::Select)
                    }
                    _ => {}
                }
            }
            _ => {}
        }
    }

    fn about_to_wait(&mut self, _event_loop: &ActiveEventLoop) {
        if let Some(gameboy) = &mut self.gameboy {
            if let Err(e) = gameboy.update_joypad_state(&self.joypad) {
                eprintln!("Joypad update error: {:?}", e);
            }
            if let Err(e) = gameboy.step() {
                eprintln!("GameBoy step error: {:?}", e);
            }
        }
        if let Some(window) = self.window {
            window.request_redraw();
        }
    }
}

fn main() -> Result<()> {
    // Initialize logs
    initialize_logs()?;

    // Get ROM path from command line arguments or use default
    let args: Vec<String> = std::env::args().collect();
    let rom_path = if args.len() > 1 {
        &args[1]
    } else {
        "roms/rom.gb"
    };

    // 讀取 ROM 檔案
    let mut rom_file =
        File::open(rom_path).map_err(|e| Error::Hardware(HardwareError::Display(e.to_string())))?;
    let mut rom_data = Vec::new();
    rom_file
        .read_to_end(&mut rom_data)
        .map_err(|e| Error::Hardware(HardwareError::Display(e.to_string())))?;

    // 建立 winit event loop 並啟動 EmulatorApp
    let event_loop = EventLoop::new().unwrap();
    let mut app = EmulatorApp::new(rom_data);
    event_loop.run_app(&mut app).expect("Failed to run app");
    Ok(())
}

fn initialize_logs() -> Result<()> {
    println!("Creating log directory and files...");
    let _ = fs::create_dir_all("logs");

    // Initialize log files
    use std::io::Write;
    if let Ok(mut file) = std::fs::OpenOptions::new()
        .create(true)
        .append(true)
        .open("logs/emulator.log")
    {
        writeln!(file, "[{}] Emulator started", chrono::Local::now())?;
    }

    Ok(())
}
