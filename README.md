# Rust Game Boy Emulator (Vibe Coding Project)

A small, just-for-fun Game Boy emulator written in Rust with an SDL3 front end. It aims to run classic ROMs, show pixels, take input, and produce some sound — with a simple, readable codebase. Think of it as a vibe coding project rather than a cycle-accurate implementation.

- CPU, bus/MMU with MBC1/3/5
- PPU with background/window/sprites and integer-scaled output
- Joypad mapped to keyboard
- Minimal APU (square channels CH1/CH2) + SDL audio
- Performance telemetry in the console (speed/FPS/cycles)

> Important: Please use your own legally obtained ROMs. No ROMs are provided.

## Quick start

1) Put your ROM(s) in the `roms/` folder. The default filename it looks for first is `roms/rom.gb`.
   The program also tries these fallbacks in order:
   - `pokemon_yellow.gb`
   - `Tetris (Japan) (En).gb`
   - `dmg-acid2.gb`

2) Build and run (Windows, `cmd.exe`):

```cmd
cargo run
```

3) Run a specific ROM, with optional speed and volume:

```cmd
cargo run -- --speed 1.00 --volume 0.35 roms\your_game.gb
```

- `--speed <factor>`: overall tempo multiplier; `1.0` is real time (default). Use `0.98`–`1.02` for fine tuning.
- `--volume <0..2>`: extra software master volume; default is `0.35` to avoid clipping.

During execution, the console prints a periodic summary like:

```
Perf: 1.00x | 59.7 FPS | 4,194,304 cycles/s
```

## Controls

- D-Pad: Arrow keys
- A: Z
- B: X
- Select: Right Shift
- Start: Enter
- Quit: Escape

## Requirements

- Rust (stable) and Cargo
- Windows (MSVC toolchain). Other OSes may work but are not the primary target here.
- SDL3 runtime (DLL or system package). This repo ships `SDL3lib/SDL3.lib` for linking, but you still need the SDL3 runtime on your machine.

### Install SDL3 (Windows)

1) Download SDL3 prebuilt binaries (Development Libraries) for Windows from the official site:
   - https://github.com/libsdl-org/SDL/releases (look for the latest SDL3 `SDL3-*.zip` for Windows)

2) Extract the archive and locate `SDL3.dll` (usually under `SDL3-*/lib/x64/SDL3.dll`).

3) Place `SDL3.dll` in one of these locations:
   - Next to the compiled executable (easiest). After a build, that is typically `target\debug\gameboy_emulator.exe` — copy `SDL3.dll` to `target\debug\`.
   - Or add the folder containing `SDL3.dll` to your `PATH`.

If you see an error about missing `SDL3.dll`, it means the runtime DLL is not being found—place/copy it next to the exe.

### Install SDL3 (Linux/macOS)

- Linux: use your distro packages (names may vary): `libsdl3`, `libsdl3-dev` or similar. On Debian/Ubuntu it may still be in newer repos or via `libsdl3-dev` from a PPA. Alternatively, build from source: https://github.com/libsdl-org/SDL
- macOS: install via Homebrew if available `brew install sdl3`, or build from source.

## What works (brief)

- CPU instruction core with basic timing and interrupts
- MBC1 / MBC3 (with simple RTC stub) / MBC5 banking
- PPU background/window/sprites, 8×16 sprites, simple priority
- Joypad `FF00` selection semantics (independent dpad/buttons rows)
- Minimal APU for CH1/CH2 (square waves). Respects NR50/NR51/NR52 basics
- Real-time pacing to ~59.73 FPS and cycle-based throttling

## Known limitations

- APU is intentionally simplified: no CH3 (wave), no CH4 (noise), no full envelopes/length/sweep/frame-sequencer yet
- PPU timing is simplified relative to test ROMs
- No save battery serialization, and RTC is a stub
- No BIOS; the emulator sets post-BIOS defaults directly

## Project layout

- `src/` — main emulator implementation (CPU/MMU/PPU), SDL display and audio backends
- `roms/` — put your `.gb` files here (not included)
- `docs/` — extra notes (`PROJECT_OVERVIEW.md`)
- `SDL3lib/` — SDL3 import lib for Windows linking

## Troubleshooting

- "No ROM found" — ensure you placed a `.gb` file under `roms/` as `roms/rom.gb`, or pass a path after `--`.
- "Missing SDL3.dll" — download SDL3, copy `SDL3.dll` next to the executable or set it in your PATH (see Install SDL3 section).
- White/blank screen — verify the ROM actually writes to LCDC and VRAM; check the console logs for LCDC first-write messages. Try a known-good test like `dmg-acid2.gb`.
- Audio too loud/quiet — adjust `--volume`. If tempo feels off, adjust `--speed` slightly.

## Notes on accuracy

This is a learning/vibe project first. It prioritizes approachability and creating a responsive experience over complete hardware accuracy. Over time, more accurate APU/PPU details may be added.

## References

- Pan Docs (specs): https://gbdev.io/pandocs/Specifications.html

## License

TBD. If you plan to reuse parts, please open an issue or drop a note.

