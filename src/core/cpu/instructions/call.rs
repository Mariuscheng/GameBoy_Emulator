use crate::core::cpu::CPU;
use crate::core::cpu::instructions::register_utils::FlagOperations;
use crate::core::cycles::CyclesType;
use crate::core::error::Result;

pub fn dispatch(cpu: &mut CPU, opcode: u8) -> Result<CyclesType> {
    match opcode {
        0xCD => call_nn(cpu),                                // CALL nn
        0xC4 => call_cc_nn(cpu, !cpu.registers.get_zero()),  // CALL NZ,nn
        0xCC => call_cc_nn(cpu, cpu.registers.get_zero()),   // CALL Z,nn
        0xD4 => call_cc_nn(cpu, !cpu.registers.get_carry()), // CALL NC,nn
        0xDC => call_cc_nn(cpu, cpu.registers.get_carry()),  // CALL C,nn
        0xC7 => rst(cpu, 0x00),                              // RST 00H
        0xCF => rst(cpu, 0x08),                              // RST 08H
        0xD7 => rst(cpu, 0x10),                              // RST 10H
        0xDF => rst(cpu, 0x18),                              // RST 18H
        0xE7 => rst(cpu, 0x20),                              // RST 20H
        0xEF => rst(cpu, 0x28),                              // RST 28H
        0xF7 => rst(cpu, 0x30),                              // RST 30H
        0xFF => rst(cpu, 0x38),                              // RST 38H
        _ => unreachable!(),
    }
}

pub fn call_nn(cpu: &mut CPU) -> Result<CyclesType> {
    // 讀取目標地址
    let low = cpu.fetch_byte()?;
    let high = cpu.fetch_byte()?;
    let target = u16::from_le_bytes([low, high]);

    // 保存當前PC到堆疊
    let pc = cpu.get_pc();
    let sp = cpu.registers.get_sp();
    cpu.write_byte(sp.wrapping_sub(1), ((pc >> 8) & 0xFF) as u8)?;
    cpu.write_byte(sp.wrapping_sub(2), (pc & 0xFF) as u8)?;
    cpu.registers.set_sp(sp.wrapping_sub(2));

    // 跳轉到目標地址
    cpu.set_pc(target);

    Ok(24) // CALL指令需要24個機器週期
}

pub fn call_cc_nn(cpu: &mut CPU, condition: bool) -> Result<CyclesType> {
    // 讀取目標地址
    let low = cpu.fetch_byte()?;
    let high = cpu.fetch_byte()?;
    let target = u16::from_le_bytes([low, high]);

    if condition {
        // 條件成立，執行CALL
        let pc = cpu.get_pc();
        let sp = cpu.registers.get_sp();
        cpu.write_byte(sp.wrapping_sub(1), ((pc >> 8) & 0xFF) as u8)?;
        cpu.write_byte(sp.wrapping_sub(2), (pc & 0xFF) as u8)?;
        cpu.registers.set_sp(sp.wrapping_sub(2));
        cpu.set_pc(target);
        Ok(24) // 條件成立時需要24個機器週期
    } else {
        Ok(12) // 條件不成立時只需要12個機器週期
    }
}

pub fn rst(cpu: &mut CPU, vector: u16) -> Result<CyclesType> {
    // 保存當前PC到堆疊
    let pc = cpu.get_pc();
    let sp = cpu.registers.get_sp();
    cpu.write_byte(sp.wrapping_sub(1), ((pc >> 8) & 0xFF) as u8)?;
    cpu.write_byte(sp.wrapping_sub(2), (pc & 0xFF) as u8)?;
    cpu.registers.set_sp(sp.wrapping_sub(2));

    // 跳轉到指定的向量地址
    cpu.set_pc(vector);

    Ok(16) // RST指令需要16個機器週期
}
