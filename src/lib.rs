#![forbid(unsafe_code)]
// Game Boy Emulator Main Module

pub mod core;
pub mod interface;
pub mod utils;

// Re-export essential error types
pub use core::error::{Error, HardwareError, InstructionError};
pub type Result<T> = std::result::Result<T, Error>;

use std::cell::RefCell;
use std::rc::Rc;

// Import emulator core types and traits
use crate::core::cpu::instructions::control::ControlInstructions;

use crate::core::cpu::CPU;
use crate::core::mmu::MMU;
use crate::core::ppu::PPU;

use crate::interface::audio::AudioInterface;
use crate::interface::input::joypad::Joypad;
use crate::interface::video::VideoInterface;

#[derive(Debug)]
pub struct GameBoy {
    mmu: Rc<RefCell<MMU>>,
    ppu: PPU,
    cpu: CPU,
    render_count: u64,
}

impl GameBoy {
    /// 取得 CPU 參考（僅供 log/debug 用，不建議遊戲邏輯直接操作）
    pub fn get_cpu_ref(&self) -> Option<&CPU> {
        Some(&self.cpu)
    }

    /// 取得 Rc<RefCell<MMU>> 參考（僅供 log/debug 用，不建議遊戲邏輯直接操作）
    pub fn get_mmu_ref(&self) -> Rc<RefCell<MMU>> {
        Rc::clone(&self.mmu)
    }

    pub fn new(
        video: Box<dyn VideoInterface>,
        _audio: Option<Box<dyn AudioInterface>>,
    ) -> Result<Self> {
        let mmu = Rc::new(RefCell::new(MMU::new()));
        let ppu = PPU::new(Rc::clone(&mmu), video);
        let cpu = CPU::new(Rc::clone(&mmu));
        Ok(Self {
            mmu,
            ppu,
            cpu,
            render_count: 0,
        })
    }

    pub fn load_rom(&mut self, rom_data: Vec<u8>) -> Result<()> {
        // 載入 ROM 到 MMU
        {
            let mut mmu = self.mmu.borrow_mut();
            mmu.load_rom(rom_data)?;

            // 初始化硬體狀態
            // LCD 寄存器
            mmu.write_byte(0xFF40, 0x91).ok(); // 啟用 LCD 和背景
            mmu.write_byte(0xFF47, 0xE4).ok(); // 設置調色板
            mmu.write_byte(0xFF42, 0).ok(); // SCY = 0
            mmu.write_byte(0xFF43, 0).ok(); // SCX = 0
            log::info!("LCD registers initialized - LCDC=0x91, BGP=0xE4");

            // 中斷狀態
            mmu.write_byte(0xFFFF, 0x00).ok(); // 禁用所有中斷
            mmu.write_byte(0xFF0F, 0x00).ok(); // 清除所有中斷標誌

            // 定時器
            mmu.write_byte(0xFF04, 0x00).ok(); // 重置除頻器
            mmu.write_byte(0xFF05, 0x00).ok(); // 重置定時器計數器
            mmu.write_byte(0xFF06, 0x00).ok(); // 重置定時器模數
            mmu.write_byte(0xFF07, 0x00).ok(); // 停止定時器
        }

        // 顯示硬體狀態
        {
            let mmu = self.mmu.borrow();
            log::debug!("[硬體狀態] CPU PC: 0x{:04X}", self.cpu.get_pc());
            log::debug!(
                "[硬體狀態] LCDC: 0x{:02X}",
                mmu.read_byte(0xFF40).unwrap_or(0)
            );
            log::debug!(
                "[硬體狀態] BGP: 0x{:02X}",
                mmu.read_byte(0xFF47).unwrap_or(0)
            );
            log::debug!(
                "[硬體狀態] IF: 0x{:02X}",
                mmu.read_byte(0xFF0F).unwrap_or(0)
            );
            log::debug!(
                "[硬體狀態] IE: 0x{:02X}",
                mmu.read_byte(0xFFFF).unwrap_or(0)
            );
        }

        Ok(())
    }

    /// 執行一個 CPU 週期並返回消耗的週期數
    pub fn step(&mut self) -> Result<u32> {
        // 獲取當前 PC 和狀態
        let pc = self.cpu.get_pc();

        // 記錄當前執行的指令位置
        if (0x0100..=0x0150).contains(&pc) {
            log::debug!("執行位置 PC=0x{:04X}", pc);
        }

        // 讀取操作碼
        let opcode = {
            let mmu = self.mmu.borrow();
            match mmu.read_byte(pc) {
                Ok(op) => op,
                Err(e) => {
                    log::error!("無法讀取地址 0x{:04X} 的操作碼: {:?}", pc, e);
                    return Err(e.into());
                }
            }
        };

        // 執行指令並記錄結果
        let mut cycles = match self.cpu.decode_and_execute(opcode) {
            Ok(c) => c,
            Err(e) => {
                // 如果是未知指令錯誤，增加 PC 並繼續執行
                if let core::error::Error::Instruction(
                    core::error::InstructionError::InvalidOpcode(_),
                ) = e
                {
                    log::warn!("未知指令 0x{:02X} at PC=0x{:04X}, 跳過執行", opcode, pc);
                    self.cpu.set_pc(pc.wrapping_add(1));
                    4 // 假設消耗 4 個週期
                } else {
                    log::error!("指令執行錯誤: {:?} at PC=0x{:04X}", e, pc);
                    return Err(e.into());
                }
            }
        };

        // 更新定時器和其他硬件狀態
        while cycles > 0 {
            // PPU 更新
            self.ppu.step(4)?;
            cycles -= 4;
        }

        Ok(4) // 返回消耗的週期數
    }

    /// 渲染畫面
    pub fn render(&mut self) -> Result<()> {
        self.ppu.render()?;
        self.render_count += 1;
        Ok(())
    }

    /// 更新手柄狀態
    pub fn update_joypad_state(&mut self, joypad: &dyn Joypad) -> Result<()> {
        if let Ok(mut mmu) = self.mmu.try_borrow_mut() {
            mmu.update_joypad_state(joypad);
        }
        Ok(())
    }

    /// 設置 CPU 的程序計數器 (PC)
    pub fn set_pc(&mut self, address: u16) -> Result<()> {
        self.cpu.set_pc(address);
        log::info!("CPU PC 設置為 0x{:04X}", address);
        Ok(())
    }

    /// 設置初始寄存器狀態
    pub fn set_initial_registers(&mut self) -> Result<()> {
        // 使用現有的 MMU 引用重新初始化 CPU
        let mmu = Rc::clone(&self.mmu);
        self.cpu = CPU::new(mmu);
        log::info!("CPU 寄存器已初始化");
        Ok(())
    }

    /// 重置 CPU 和內存狀態
    pub fn reset(&mut self) -> Result<()> {
        if let Ok(mut mmu) = self.mmu.try_borrow_mut() {
            mmu.reset();
        }
        self.cpu.reset()?;
        log::info!("GameBoy 系統已重置");
        Ok(())
    }

    pub fn get_video_mut(&mut self) -> &mut dyn VideoInterface {
        self.ppu.get_video_mut()
    }

    /// 取得目前 framebuffer，回傳 Vec<u8> (RGB)
    pub fn get_framebuffer(&self) -> Vec<u8> {
        self.ppu.display.get_buffer()
    }

    /// 計算 VRAM內容的雜湊值
    pub fn get_vram_hash(&self) -> Result<u64> {
        use std::collections::hash_map::DefaultHasher;
        use std::hash::{Hash, Hasher};

        let vram_data = self.get_vram_state()?;
        let mut hasher = DefaultHasher::new();
        vram_data.hash(&mut hasher);
        Ok(hasher.finish())
    }

    /// 轉儲 VRAM 內容
    pub fn dump_vram(&self) -> Result<Vec<u8>> {
        self.get_vram_state()
    }

    // 取得 VRAM 狀態的快照（用於除錯）
    pub fn get_vram_state(&self) -> Result<Vec<u8>> {
        let mut vram_state = Vec::with_capacity(0x2000); // 8KB VRAM
        if let Ok(mmu) = self.mmu.try_borrow() {
            for addr in 0x8000..=0x9FFF {
                if let Ok(value) = mmu.read_byte(addr) {
                    vram_state.push(value);
                }
            }
        }
        Ok(vram_state)
    }

    // 取得當前 PC 值
    pub fn get_pc(&self) -> Option<u16> {
        Some(self.cpu.get_pc())
    }

    // 增加 PC 計數器
    pub fn increment_pc(&mut self, amount: u16) -> Result<()> {
        let new_pc = self.cpu.get_pc().wrapping_add(amount);
        self.cpu.set_pc(new_pc);
        Ok(())
    }

    // 手動執行 PUSH AF 指令
    pub fn manual_push_af(&mut self) -> Result<()> {
        Ok(self.cpu.push_af().map(|_| ())?.into())
    }

    pub fn execute_instruction(&mut self, opcode: u8) -> Result<()> {
        let cycles = self.cpu.decode_and_execute(opcode)?;
        self.update_timer(cycles)?;
        self.ppu.step(cycles)?;
        Ok(())
    }

    fn update_timer(&mut self, cycles: u32) -> Result<()> {
        if let Ok(mut mmu) = self.mmu.try_borrow_mut() {
            // DIV register (0xFF04) - 增加每個 M-cycle
            let div = mmu.io_registers[0x04].wrapping_add((cycles / 4) as u8);
            mmu.io_registers[0x04] = div;

            // 檢查計時器是否啟用
            let tac = mmu.io_registers[0x07];
            if tac & 0x04 != 0 {
                // 計算速度
                let speed = match tac & 0x03 {
                    0 => 1024, // 4096 Hz
                    1 => 16,   // 262144 Hz
                    2 => 64,   // 65536 Hz
                    3 => 256,  // 16384 Hz
                    _ => unreachable!(),
                };

                // 更新 TIMA (0xFF05)
                let tima = mmu.io_registers[0x05].wrapping_add((cycles / speed) as u8);
                if tima == 0 {
                    // TIMA 溢位，載入 TMA (0xFF06) 並設置計時器中斷
                    mmu.io_registers[0x05] = mmu.io_registers[0x06];
                    mmu.interrupt_flags |= 0x04;
                } else {
                    mmu.io_registers[0x05] = tima;
                }
            }
            Ok(())
        } else {
            Err(Error::Hardware(core::error::HardwareError::BorrowError))
        }
    }
}
