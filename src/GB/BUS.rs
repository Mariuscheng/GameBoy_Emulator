use crate::GB::{bus_apu::BusAPU, bus_io::BusIO, bus_mem::BusMem};

pub struct Bus {
    pub bus_mem: BusMem,
    pub bus_io: BusIO,
    pub bus_apu: BusAPU,
}

impl Bus {
    pub fn new() -> Self {
        Self {
            bus_mem: BusMem::new(),
            bus_io: BusIO::new(),
            bus_apu: BusAPU::new(),
        }
    }

    // --- 對外委派方法 ---
    pub fn read(&self, addr: u16) -> u8 {
        self.bus_mem.read(addr, &self.bus_io, &self.bus_apu)
    }
    pub fn write(&mut self, addr: u16, val: u8) {
        self.bus_mem
            .write(addr, val, &mut self.bus_io, &mut self.bus_apu)
    }
    pub fn step(&mut self, cycles: u64) {
        self.bus_io
            .step(cycles, &mut self.bus_mem, &mut self.bus_apu)
    }
    pub fn is_dma_active(&self) -> bool {
        self.bus_io.is_dma_active()
    }
    pub fn get_ie_raw(&self) -> u8 {
        self.bus_io.get_ie_raw()
    }
    pub fn get_if_raw(&self) -> u8 {
        self.bus_io.get_if_raw()
    }
    pub fn set_if_raw(&mut self, v: u8) {
        self.bus_io.set_if_raw(v)
    }
    pub fn load_rom(&mut self, data: Vec<u8>) {
        self.bus_mem.load_rom(data)
    }
    pub fn attach_synth(
        &mut self,
        synth: std::sync::Arc<std::sync::Mutex<crate::interface::audio::SimpleAPUSynth>>,
    ) {
        self.bus_apu.attach_synth(synth)
    }
    pub fn set_joypad_rows(&mut self, dpad: u8, btns: u8) {
        self.bus_io.set_joypad_rows(dpad, btns)
    }
    pub fn framebuffer(&self) -> &[u8] {
        self.bus_io.framebuffer()
    }
}
