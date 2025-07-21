pub mod arithmetic;
pub mod bit;
pub mod cb;
pub mod control;
pub mod cpu;
pub mod cycles;
pub mod flags;
pub mod io;
pub mod jump;
pub mod load;
pub mod logic;
pub mod prelude;
pub mod register_utils;
pub mod registers;
pub mod stack;
pub mod timer;
pub mod undocumented;

pub fn execute(
    cpu_ref: &mut crate::core::cpu::cpu::CPU,
    opcode: u8,
) -> crate::core::cycles::CyclesType {
    // PC 溢位防呆
    let rom_len = cpu_ref.mmu().borrow().cartridge_rom.len() as u16;
    if cpu_ref.registers.pc >= rom_len || cpu_ref.registers.pc > 0xFFFE {
        #[cfg(test)]
        #[path = "tests.rs"]
        mod tests;
        return 0;
    }
    // 非法操作碼判斷（只 debug print，不終止循環）
    if [
        0xD3, 0xDB, 0xDD, 0xE3, 0xE4, 0xEB, 0xEC, 0xED, 0xF4, 0xFC, 0xFD,
    ]
    .contains(&opcode)
    {
        return 4;
    }
    match opcode {
        0x00 => 4,
        0x08 => load::dispatch(cpu_ref, opcode).unwrap_or(0),
        0x01 | 0x11 | 0x21 | 0x31 => load::dispatch(cpu_ref, opcode).unwrap_or(0),
        0x02 | 0x12 | 0x22 | 0x32 => load::dispatch(cpu_ref, opcode).unwrap_or(0),
        0x06 | 0x0E | 0x16 | 0x1E | 0x26 | 0x2E | 0x36 | 0x3E => {
            load::dispatch(cpu_ref, opcode).unwrap_or(0)
        }
        0x0A | 0x1A | 0x2A | 0x3A => load::dispatch(cpu_ref, opcode).unwrap_or(0),
        0x40..=0x75 | 0x77..=0x7F => load::dispatch(cpu_ref, opcode).unwrap_or(0),
        0x03 | 0x13 | 0x23 | 0x33 => arithmetic::dispatch(cpu_ref, opcode).unwrap_or(0),
        0x0B | 0x1B | 0x2B | 0x3B => arithmetic::dispatch(cpu_ref, opcode).unwrap_or(0),
        0x04 | 0x0C | 0x14 | 0x1C | 0x24 | 0x2C | 0x34 | 0x3C => {
            arithmetic::dispatch(cpu_ref, opcode).unwrap_or(0)
        }
        0x05 | 0x0D | 0x15 | 0x1D | 0x25 | 0x2D | 0x35 | 0x3D => {
            arithmetic::dispatch(cpu_ref, opcode).unwrap_or(0)
        }
        0x10 => control::dispatch(cpu_ref, opcode).unwrap_or(0),
        0x76 => control::dispatch(cpu_ref, opcode).unwrap_or(0),
        0x18 => jump::dispatch(cpu_ref, opcode).unwrap_or(0),
        0x20 | 0x28 | 0x30 | 0x38 => jump::dispatch(cpu_ref, opcode).unwrap_or(0),
        // JP/CALL/RET 跳躍指令補齊 PC 處理
        0xC3 => {
            // JP nn (opcode=0xC3) 取 ROM 兩個 byte 作為目標位址
            let lo = cpu_ref
                .mmu()
                .borrow()
                .read_byte(cpu_ref.registers.pc + 1)
                .unwrap_or(0);
            let hi = cpu_ref
                .mmu()
                .borrow()
                .read_byte(cpu_ref.registers.pc + 2)
                .unwrap_or(0);
            let addr = ((hi as u16) << 8) | (lo as u16);
            cpu_ref.registers.pc = addr;
            12
        }
        0xC9 => {
            // RET (opcode=0xC9) 暫時 PC+=1
            cpu_ref.registers.pc += 1;
            8
        }
        0xCD => {
            // CALL nn (opcode=0xCD) 取 ROM 兩個 byte 作為目標位址
            let lo = cpu_ref
                .mmu()
                .borrow()
                .read_byte(cpu_ref.registers.pc + 1)
                .unwrap_or(0);
            let hi = cpu_ref
                .mmu()
                .borrow()
                .read_byte(cpu_ref.registers.pc + 2)
                .unwrap_or(0);
            let addr = ((hi as u16) << 8) | (lo as u16);
            cpu_ref.registers.pc = addr;
            24
        }
        // 其他跳躍指令暫時 PC+=1
        0xC2 | 0xCA | 0xD2 | 0xDA | 0xE9 | 0xC4 | 0xCC | 0xD4 | 0xDC | 0xC0 | 0xC8 | 0xD0
        | 0xD8 | 0xD9 => {
            cpu_ref.registers.pc += 1;
            4
        }
        0xA0..=0xA7 => logic::dispatch(cpu_ref, opcode).unwrap_or(0),
        0xE6 => logic::dispatch(cpu_ref, opcode).unwrap_or(0),
        0xB0..=0xB7 => logic::dispatch(cpu_ref, opcode).unwrap_or(0),
        0xF6 => logic::dispatch(cpu_ref, opcode).unwrap_or(0),
        0xA8..=0xAF => logic::dispatch(cpu_ref, opcode).unwrap_or(0),
        0xEE => logic::dispatch(cpu_ref, opcode).unwrap_or(0),
        0xB8..=0xBF => logic::dispatch(cpu_ref, opcode).unwrap_or(0),
        0xFE => logic::dispatch(cpu_ref, opcode).unwrap_or(0),
        // CB 前綴指令交由 bit module 處理
        0xCB => {
            let next_opcode = cpu_ref
                .mmu()
                .borrow()
                .read_byte(cpu_ref.registers.pc + 1)
                .unwrap_or(0);
            bit::dispatch(cpu_ref, next_opcode).unwrap_or(0)
        }
        // 其他未覆蓋的指令自動分派到各指令族
        _ => {
            // 依 opcode 範圍自動分派
            if opcode >= 0x40 && opcode <= 0x7F {
                load::dispatch(cpu_ref, opcode).unwrap_or(4)
            } else if opcode >= 0x80 && opcode <= 0xBF {
                arithmetic::dispatch(cpu_ref, opcode).unwrap_or(4)
            } else if opcode >= 0xA0 && opcode <= 0xFF {
                logic::dispatch(cpu_ref, opcode).unwrap_or(4)
            } else if opcode >= 0xC0 && opcode <= 0xFF {
                control::dispatch(cpu_ref, opcode).unwrap_or(4)
            } else if opcode >= 0xE0 && opcode <= 0xFF {
                io::dispatch(cpu_ref, opcode).unwrap_or(4)
            } else if opcode >= 0xCB {
                bit::dispatch(cpu_ref, opcode).unwrap_or(4)
            } else {
                4
            }
        }
    }
}
