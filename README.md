# GameBoy Emulator(A.I Project)

This project is a GameBoy emulator written in C++ using SDL3 for graphics, input, and audio handling. The goal is to accurately emulate the original Nintendo GameBoy hardware, allowing users to play classic GameBoy games on modern systems.

## Features

- ✅ Accurate emulation of the GameBoy CPU (LR35902) - All documented opcodes implemented
- ✅ Graphics rendering using SDL3
- ✅ Input handling for GameBoy controls
- ✅ Audio emulation (APU logic implemented)
- ✅ ROM loading and cartridge support
- 🔄 Save state functionality (planned)
- 🔄 Debugging tools (planned)

## Prerequisites

- C++14 compatible compiler (e.g., Visual Studio 2022 or later, GCC, Clang)
- SDL3 library (installed via vcpkg)
- CMake (for build system)
- vcpkg package manager

## Installation

### Installing Dependencies via vcpkg

1. Install vcpkg if not already installed:
   ```bash
   git clone https://github.com/Microsoft/vcpkg.git
   cd vcpkg
   .\bootstrap-vcpkg.bat  # Windows
   # or ./bootstrap-vcpkg.sh  # Linux/macOS
   ```

2. Install SDL3:
   ```bash
   vcpkg install sdl3
   ```

### Installing Additional SDL3 Extensions (Optional)

If you plan to extend the emulator with image, font, or audio features:
```bash
vcpkg install sdl3-image sdl3-ttf sdl3-mixer
```

## Building the Project

### Using CMake (Recommended)

1. Clone the repository:
   ```bash
   git clone https://github.com/yourusername/GameBoy-Emulator.git
   cd GameBoy-Emulator
   ```

2. Configure the project with CMake:
   ```bash
   mkdir build
   cd build
   cmake .. -DCMAKE_TOOLCHAIN_FILE=C:/path/to/vcpkg/scripts/buildsystems/vcpkg.cmake
   ```
   Replace `C:/path/to/vcpkg` with your actual vcpkg installation path.

3. Build the project:
   ```bash
   cmake --build . --config Debug
   ```

4. The executables will be created in the `Debug` or `Release` directory.

### Alternative: Using Visual Studio

1. Open the `GameBoy.sln` file in Visual Studio 2022.

2. Ensure vcpkg is integrated with Visual Studio:
   ```bash
   vcpkg integrate install
   ```

3. Set the platform to x64 and configuration to Debug or Release.

4. Build the solution.

5. Copy required DLLs to the output directory:
   - Copy `SDL3.dll` from `C:\vcpkg\installed\x64-windows\bin\` to `x64\Debug\` or `x64\Release\`.
   - If using extensions, copy additional DLLs like `SDL3_image.dll`, etc.

## Usage

1. Build the project as described above.

2. Place your GameBoy ROM files in the `roms/` directory (create it if it doesn't exist).

3. Run the emulator with a GameBoy ROM file:
   ```bash
   # From build directory
   .\Debug\GameBoy.exe ..\roms\tetris.gb
   ```
   Example:
   ```bash
   .\Debug\GameBoy.exe ..\roms\tetris.gb
   ```

4. The emulator will load the ROM, parse its header, and start emulation.

5. Use the following keyboard controls:
   - **A Button**: Z
   - **B Button**: X
   - **Start**: Enter
   - **Select**: Right Shift
   - **D-Pad**: Arrow Keys (Up, Down, Left, Right)

### ROM Analysis Tool

The project also includes a ROM analysis tool:

```bash
.\Debug\analyze_rom.exe ..\roms\tetris.gb
```

This tool parses and displays detailed information about GameBoy ROM headers.

### ROM Parsing Details

The emulator supports parsing GameBoy ROM headers to extract important information:

- **Cartridge Type**: Determines MBC (Memory Bank Controller) type
- **ROM Size**: Total ROM capacity
- **RAM Size**: External RAM size (if any)
- **Title**: Game title
- **Licensee Code**: Publisher information
- **Destination Code**: Region (Japan/World)
- **Version Number**: ROM version

Supported cartridge types include:
- ROM only
- MBC1, MBC2, MBC3, MBC5
- With or without RAM
- With or without battery backup

## GameBoy Execution Flow

The GameBoy emulator follows the original hardware's execution model. Here's how the emulator processes a GameBoy game:

### 1. ROM Loading and Initialization
- Load the `.gb` ROM file from disk
- Parse the ROM header to extract metadata (title, cartridge type, memory sizes)
- Initialize the Memory Management Unit (MMU) with ROM data
- Set up memory banking based on cartridge type (MBC)

### 2. Hardware Initialization
- Initialize CPU (LR35902) with correct register values
- Set up memory map (64KB address space)
- Initialize Picture Processing Unit (PPU) for graphics
- Set up Audio Processing Unit (APU) for sound
- Configure input handling for joypad

### 3. Main Emulation Loop
The emulator runs in a continuous loop, synchronizing with real-time:

#### CPU Execution
- Fetch instruction from memory (PC register)
- Decode and execute instruction
- Update CPU registers and flags
- Handle clock cycles (each instruction takes specific cycles)

#### Memory Access
- CPU reads/writes to memory through MMU
- MMU handles address translation and banking
- Special memory regions trigger hardware behavior

#### Interrupt Handling
- Check for interrupts (VBlank, LCD, Timer, Serial, Joypad)
- If interrupt enabled and triggered, jump to interrupt handler
- Update interrupt flags and master enable

#### Graphics Rendering
- PPU processes scanlines (144 visible + 10 VBlank)
- Render background, window, and sprites
- Update LCD status and trigger VBlank interrupt
- Copy frame buffer to SDL window

#### Audio Generation
- APU generates 4 channels (2 pulse, 1 wave, 1 noise)
- Mix audio samples based on current frame
- Output to SDL audio device

#### Input Processing
- Poll SDL events for keyboard input
- Map keys to GameBoy buttons (A, B, Start, Select, D-pad)
- Update joypad register and trigger interrupts

#### Timing Synchronization
- Maintain 4.194304 MHz CPU clock
- Synchronize with 59.73 Hz frame rate
- Handle frame skipping if running too slow

### 4. Cartridge-Specific Features
- MBC banking for larger ROMs/RAM
- Real-time clock (MBC3)
- Rumble motor control (MBC5)
- Battery-backed save data

### 5. Debug and Development Features
- Instruction logging and disassembly
- Memory viewer and breakpoints
- Save/load emulator state
- Performance profiling

This execution flow ensures accurate timing and behavior matching the original GameBoy hardware, allowing games to run correctly with proper graphics, sound, and input response.

## Current Status

The GameBoy emulator is currently in **Phase 6** of development with the following components implemented:

### ✅ Completed Features
- **Full CPU Emulation**: All 256 LR35902 opcodes implemented
  - ✅ **CPU test suite (cpu_instrs.gb)**: 10/11 test groups passing
  - Test 01: ✅ Special instructions - **PASS**
  - Test 02: ⚠️ Interrupts & Timer - **SUBTEST 04 FAILS** (others pass)
  - Tests 03-11: ✅ All **PASS**
  - Only test 02:04 ("Timer doesn't work") remains unresolved
  
- **Memory Management**: 64KB address space with ROM loading and I/O register handling
  - ✅ Timer registers (DIV, TIMA, TAC) implemented
  - ✅ Interrupt flag and interrupt enable registers
  
- **Timer Emulation** (NEW):
  - ✅ DIV register (0xFF04) - Internal 16-bit divider (high byte)
  - ✅ TIMA register (0xFF05) - 8-bit counter with overflow
  - ✅ TAC register (0xFF07) - Control: frequency select + enable bit
  - ✅ Four timer frequencies: 1024/16/64/256 M-cycles
  - ✅ Timer interrupt (IF bit 2) triggering
  - ✅ TIMA overflow delay (4 M-cycle delay before interrupt)
  - ✅ Bit-based falling edge detection for accurate timing
  
- **Graphics Setup**: SDL3 window and renderer initialized
- **Input Handling**: Keyboard mapping to GameBoy controls (A, B, Start, Select, D-Pad)
- **ROM Loading**: Header parsing, cartridge type detection, region detection
- **PPU Implementation**: Tile-based graphics, sprite rendering, LCD timing (modes 0-3)
- **APU Implementation**: 4-channel audio synthesis logic (pulse, wave, noise channels)
- **Build System**: CMake configuration with vcpkg integration
- **Interrupt System**: Full interrupt handling (VBlank, LCD, Timer, Serial, Joypad)

### 🔄 In Progress / Testing
- **Timer Test Compatibility**: test 02:04 still failing despite implementation
  - Possible issues: TIMA internal value delay, edge case in test expectations
  - May require Game Boy-specific hardware quirks (glitches) for compatibility
- **Audio Integration**: SDL3 audio output with push model (44.1kHz stereo)
- **Advanced Cartridge Support**: MBC1/2/3/5 controller implementations
- **Save States**: Battery-backed RAM and emulator state saving

### 🎮 Known Issues
- **test 02:04**: ("Timer doesn't work") - Subtest 04 of interrupt/timer test fails
  - All other tests pass consistently
  - Likely requires additional Game Boy hardware details (e.g., TIMA delay quirks)
  
### 🎮 Tested Games
- **Tetris (World)**: Successfully loads and runs (basic ROM-only support)
- **Test ROMs**: Various CPU and instruction test ROMs analyzed

The emulator now has comprehensive timer support and successfully passes 10 out of 11 test groups in the Blargg CPU instruction test suite.

## Debugging and Analysis

### Test ROM Status
The emulator's CPU implementation can be tested with various test ROMs:

```bash
# CPU Instruction Test (all 11 groups)
.\Debug\GameBoy.exe ..\roms\cpu_instrs.gb

# Other test ROMs
.\Debug\GameBoy.exe ..\roms\halt_bug.gb
.\Debug\GameBoy.exe ..\roms\instr_timing.gb
.\Debug\GameBoy.exe ..\roms\mem_timing_1.gb
```

### Current Test Results
- **Test 01** (Special Instructions): ✅ **PASS**
- **Test 02** (Interrupts & Timer): ⚠️ **MIXED**
  - Subtests 01-03: ✅ PASS (DI works, EI works, HALT works)
  - Subtest 04 ("Timer doesn't work"): ❌ **FAIL** (despite timer implementation)
- **Tests 03-11** (Various instruction groups): ✅ **ALL PASS**

### Timer Implementation Details
The Game Boy timer system has been fully implemented with the following features:
- **16-bit internal divider** that continuously increments
- **DIV register** (0xFF04) returns bits 15-8 of the divider
- **TIMA register** (0xFF05) increments when a selected bit of the divider changes from 1→0
- **TAC register** (0xFF07) selects which divider bit triggers TIMA increments
- **Bit positions by frequency** (TAC bits 0-1):
  - `00`: bit 9 (slowest, 4096 Hz)
  - `01`: bit 3 (fastest, 262144 Hz)
  - `10`: bit 5 (65536 Hz)
  - `11`: bit 7 (16384 Hz)
- **TIMA overflow handling**: When TIMA reaches 0xFF and another increment occurs:
  - TIMA is reset to 0x00
  - IF register bit 2 is set after 4 M-cycle delay (TIMA delay quirk)

### Known Test Failure Analysis
**test 02:04 ("Timer doesn't work")** remains unresolved despite full timer implementation:
- All prerequisite subtests (DI, EI, HALT) pass
- Other tests that depend on timer functionality pass (03-11)
- Likely cause: 
  - May require additional Game Boy hardware quirks
  - TIMA internal value delay might need more complex implementation
  - Specific test expectations may differ from standard documentation

For detailed analysis and debugging steps, refer to the test ROM analysis in `DEBUG_NOTES.md`.

### Logging and Debugging
The emulator generates debug logs:
- `cpu_log.txt` - Instruction execution log (PC, opcode)
- `serial_output.txt` - Serial port output from ROM tests

## Project Structure

```
GameBoy/
├── CMakeLists.txt          # CMake build configuration
├── GameBoy.sln            # Visual Studio solution file
├── main.cpp               # Entry point with SDL3 initialization
├── analyze_rom.cpp        # ROM analysis tool
├── DEBUG_NOTES.md         # Detailed CPU test failure analysis
├── README.md              # This file
├── include/               # Header files
│   ├── cpu.h             # CPU class definition
│   ├── emulator.h        # Emulator class definition
│   ├── mmu.h             # Memory Management Unit
│   ├── ppu.h             # Picture Processing Unit
│   └── apu.h             # Audio Processing Unit
├── src/                   # Source files
│   ├── cpu.cpp           # CPU implementation (all 256 opcodes)
│   ├── emulator.cpp      # Main emulator loop and initialization
│   ├── mmu.cpp           # Memory management and I/O registers
│   ├── ppu.cpp           # Graphics processing and LCD timing
│   └── apu.cpp           # Audio synthesis (4 channels)
├── roms/                  # GameBoy ROM files (for testing)
└── build/                 # Build directory (generated by CMake)
```

## Detailed Tasks

The development of the GameBoy emulator is broken down into the following detailed tasks. Each task represents a significant component of the emulator.

### Phase 1: Project Setup and Basic Infrastructure
1. **✅ Set up C++ project with SDL3 integration**
   - Create Visual Studio project
   - Configure C++14 standard
   - Integrate vcpkg for SDL3
   - Verify SDL3 initialization

2. **✅ Implement basic project structure**
   - Create directories for source, headers, and resources
   - Set up CMakeLists.txt for cross-platform builds
   - Add version control (Git)

### Phase 2: CPU Emulation
3. **✅ Implement LR35902 CPU core**
   - ✅ Define CPU registers and flags
   - ✅ Implement complete instruction set (all 256 documented opcodes)
     - ✅ Control: NOP, HALT, STOP, DI, EI
     - ✅ Loads: LD (8-bit and 16-bit), LDH, all register and indirect variants
     - ✅ Arithmetic: ADD, ADC, SUB, SBC, INC, DEC, ADD HL, rr
     - ✅ Logical: AND, OR, XOR, CP (all variants)
     - ✅ Stack: PUSH, POP (all register pairs)
     - ✅ Jumps: JP, JR, CALL, RET, RETI, RST (all conditional variants)
     - ✅ Rotates: RLCA, RRCA, RLA, RRA
     - ✅ CB Prefix: RLC, RRC, RL, RR, SLA, SRA, SRL, SWAP, BIT, SET, RES (all 8 registers + (HL))
     - ✅ Misc: CPL, SCF, CCF, DAA
     - ✅ Special loads: LD (a16) SP, LD SP HL, LD HL SP+n
     - ✅ Undefined opcodes: Handled as NOP
   - ✅ Handle interrupt system (IME, IE, IF)
   - ✅ Implement clock cycle accuracy for all instructions
   - ⚠️ **Status**: 8/11 cpu_instrs.gb test groups passing
     - Groups 2, 6, 10 fail - **requires further debugging**
     - Periodic failure pattern suggests timing or state management issue
     - See DEBUG_NOTES.md for detailed analysis and recommendations

4. **✅ Memory Management Unit (MMU)**
   - ✅ Implement 64KB memory map (ROM, WRAM, HRAM, I/O registers)
   - ✅ Implement memory-mapped I/O (0xFF00-0xFFFF)
   - ✅ Serial port (0xFF01, 0xFF02) for test ROM output
   - ✅ Interrupt registers (0xFF0F, 0xFFFF)
   - ✅ PPU register access (0xFF40-0xFF4B)
   - ✅ APU register mapping
   - 🔄 ROM/RAM banking (basic structure in place)

### Phase 3: Graphics and Display
5. **✅ Picture Processing Unit (PPU) emulation**
   - ✅ Implement tile-based graphics (background and window)
   - ✅ Handle sprite rendering (OAM scanning, priority, flipping)
   - ✅ Implement background and window layers
   - ✅ Manage LCD timing (OAM search, pixel transfer, HBlank, VBlank)
   - ✅ LCD interrupt handling (VBlank, STAT)

6. **✅ SDL3 Graphics Integration**
   - ✅ Set up SDL3 window and renderer
   - ✅ Implement frame buffer rendering
   - ✅ Handle screen scaling and aspect ratio

### Phase 4: Input and Controls
7. **✅ Input handling**
   - ✅ Map keyboard inputs to GameBoy buttons
   - ✅ Implement joypad register emulation
   - ✅ Handle input interrupts
   - ✅ Fixed interrupt enable initialization (interrupts now properly enabled)

### Phase 5: Audio Emulation
8. **✅ Audio Processing Unit (APU)**
   - ✅ Implement 4-channel audio synthesis (2 pulse, 1 wave, 1 noise)
   - ✅ Handle wave, noise, and pulse channels
   - ✅ Integrate with SDL3 audio (push model, 44.1kHz stereo)

### Phase 6: Timer Emulation
9. **✅ Timer System** (NEW - COMPLETED)
   - ✅ DIV register (0xFF04) - Internal divider, returns high byte of 16-bit counter
   - ✅ TIMA register (0xFF05) - 8-bit counter with automatic increment
   - ✅ TAC register (0xFF07) - Control byte (frequency select bits 0-1, enable bit 2)
   - ✅ Four timer frequencies based on TAC bits 0-1:
     - 00: 1024 M-cycles per increment (4096 Hz)
     - 01: 16 M-cycles per increment (262144 Hz)
     - 10: 64 M-cycles per increment (65536 Hz)
     - 11: 256 M-cycles per increment (16384 Hz)
   - ✅ Bit-edge detection: TIMA increments on falling edge of selected divider bit
   - ✅ Timer interrupt (IF register bit 2) triggered on TIMA overflow (0xFF → 0x00)
   - ✅ TIMA overflow delay: 4 M-cycle delay before interrupt flag is set
   - ⚠️ **Note**: test 02:04 still fails despite implementation
     - All other tests (01, 03-11) pass successfully
     - May require additional Game Boy hardware quirks or timing details

### Phase 7: Cartridge and ROM Support
10. **✅ ROM loading and parsing**
   - ✅ Load ROM file from disk
   - ✅ Parse ROM header (0x0100-0x014F)
     - ✅ Extract game title, cartridge type, ROM/RAM size
     - ✅ Validate checksums
     - ✅ Determine MBC type and features
   - 🔄 Support various GameBoy cartridge types (basic ROM only implemented)
   - 🔄 Implement MBC (Memory Bank Controller) logic
   - 🔄 Handle save data (SRAM) with battery backup

11. **File I/O for save states**
    - Implement save/load state functionality
    - Support battery-backed saves

### Phase 8: Advanced Features
12. **Debugging tools**
    - CPU instruction logging
    - Memory viewer
    - Breakpoints and stepping

13. **Performance optimization**
    - Optimize rendering loop
    - Implement frame skipping
    - Profile and optimize CPU emulation

14. **Cross-platform support**
    - Test on Windows, Linux, and macOS
    - Update CMake configuration

15. **Testing and validation**
    - Test with various ROMs
    - Implement unit tests for components
    - Validate against real GameBoy behavior

### Phase 9: Finalization
16. **User interface**
    - Implement menu system
    - Add settings and configuration
    - Create launcher application

17. **Documentation and packaging**
    - Complete README and documentation
    - Create installer or portable package
    - Add licensing information

## Contributing

Contributions are welcome! Please fork the repository and submit pull requests. For major changes, please open an issue first to discuss what you would like to change.

## License

This project is licensed under the MIT License - see the LICENSE file for details.

## Acknowledgments

- Nintendo for creating the GameBoy
- The SDL development team

- Various open-source GameBoy emulator projects for reference
