//! GameBoy模擬器調試工具

use crate::{error::Result, GameBoy};
use std::fs::File;
use std::io::Write;
use std::time::{Duration, Instant};

/// 用於調試的擴展功能實現
impl GameBoy {
    /// 獲取當前PC值
    pub fn get_pc(&self) -> Result<u16> {
        // 根據您的GameBoy結構實現，可能需要調整
        Ok(self.cpu.registers.pc)
    }

    /// 手動執行PUSH AF指令
    pub fn manual_push_af(&mut self) -> Result<()> {
        // 獲取AF寄存器的值
        let a = self.cpu.registers.a;
        let f = self.cpu.registers.f;
        let af = (a as u16) << 8 | (f as u16);

        // 將SP減2
        self.cpu.registers.sp = self.cpu.registers.sp.wrapping_sub(2);

        // 寫入記憶體
        self.cpu
            .write_byte(self.cpu.registers.sp + 1, (af >> 8) as u8)?;
        self.cpu
            .write_byte(self.cpu.registers.sp, (af & 0xFF) as u8)?;

        Ok(())
    }

    /// 增加PC計數器
    pub fn increment_pc(&mut self, amount: u16) -> Result<()> {
        self.cpu.registers.pc = self.cpu.registers.pc.wrapping_add(amount);
        Ok(())
    }

    /// 獲取VRAM當前狀態
    pub fn get_vram_state(&self) -> Result<Vec<u8>> {
        let mut vram_data = Vec::with_capacity(8192); // 8KB VRAM

        // 從0x8000到0x9FFF讀取VRAM
        for addr in 0x8000..=0x9FFF {
            let value = self.cpu.read_byte(addr)?;
            vram_data.push(value);
        }

        Ok(vram_data)
    }

    /// 將VRAM內容保存到文件
    pub fn dump_vram_to_file(&self, filename: &str) -> Result<()> {
        let vram_data = self.get_vram_state()?;
        let mut file = File::create(filename).map_err(|e| {
            crate::error::Error::Hardware(crate::error::HardwareError::Display(format!(
                "無法創建VRAM轉儲文件: {}",
                e
            )))
        })?;

        file.write_all(&vram_data).map_err(|e| {
            crate::error::Error::Hardware(crate::error::HardwareError::Display(format!(
                "無法寫入VRAM轉儲: {}",
                e
            )))
        })?;

        Ok(())
    }
}
