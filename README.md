# GameBoy Emulator (A.I Project)

> 一個用 C++ 與 SDL3 撰寫的 Game Boy (DMG) 模擬器。現已通過 blargg `cpu_instrs` 全部測試，`dmg-acid2` 圖像測試亦已正確顯示。

## 狀態總覽
| 子系統 | 現況 |
|--------|------|
| CPU | 指令集、旗標、EI 延遲、中斷優先順序與服務流程已通過 `cpu_instrs.gb` 全部項目 |
| PPU | 掃描線/Mode 時序 (456 cycles/line)、背景/視窗/Sprite 渲染皆正常；`dmg-acid2` 已通過（含右下 Window/遮蔽/優先序） |
| APU | 目前骨架存在，可輸出樣本（尚未與測試 ROM 嚴格比對） |
| MMU | 寄存器、IF/IE、Timer (DIV/TIMA/TMA/TAC)；已實作 CPU 端 VRAM/OAM 忙碌鎖定，並提供 `ppu_read` 供 PPU 在 Mode2/3 期間合法讀取 |
| 測試 | blargg `cpu_instrs`：Passed all tests；`dmg-acid2`：Passed |

## 已知待辦 / 未完成
1. 精細像素 FIFO 與 SCX 捲動延遲、STAT 中斷精準觸發點（acid2 已過，但為提升相容性仍建議實作）。
2. APU 聲道細節、增益/掃頻/包絡的精準化與測試。
3. 減少除錯輸出：以 compile-time 或 runtime 旗標控制（避免影響效能）。

## 主要技術特性
- C++20 (原始 README 標示 C++14，現已升級並使用現代語言特性)。
- SDL3：顯示與音訊輸出。
- 精簡 PPU 模式循環：
	- 每行 456 cycles：0–79 (Mode2 OAM)，80–251 (Mode3 Pixel Transfer)，252–455 (Mode0 HBlank)。
	- LY 0–143 可視行；144–153 VBlank (Mode1)；154 重設。
- CPU 中斷：服務 5 M-cycles（2 wait + push PC 高/低 + 跳轉），EI 延遲一指令生效。
- Sprite：OAM 原始順序選前 10 個；支援 8x16、Flip、背景優先 (color0 透明 + 背景非 0 隱藏 behind_bg)。

## 專案結構
```
GameBoy/
├── main.cpp              # 入口
├── analyze_rom.cpp       # ROM 分析工具
├── include/              # 標頭
│   ├── cpu.h / mmu.h / ppu.h / apu.h / emulator.h
├── src/                  # 實作檔
│   ├── cpu.cpp / mmu.cpp / ppu.cpp / apu.cpp / emulator.cpp
├── roms/                 # 測試與範例 ROM (acid2, cpu_instrs, tetris 等)
├── build/                # CMake/VS 產物 (生成後)
└── x64/Debug/            # Visual Studio 輸出（含 SDL3.dll）
```

## 編譯與執行
### Visual Studio (Windows)
1. 開啟 `GameBoy.sln`。
2. 設定組態 `Debug | x64`。
3. 建置後 `SDL3.dll` 會自動複製到輸出目錄。若缺失可手動放入。
4. 執行範例：
	- 若從【專案根目錄】呼叫（建議，路徑最簡單）：
```powershell
./x64/Debug/GameBoy.exe ./roms/cpu_instrs.gb
./x64/Debug/GameBoy.exe ./roms/dmg-acid2.gb
./x64/Debug/GameBoy.exe ./roms/dmg-acid2.gb 32   # 指定 LCD start offset (例：32)
```
	- 若在 `build\Debug` 資料夾內（CMake 產物目錄）：需回到兩層以上找 ROM：
```powershell
./GameBoy.exe ../../roms/cpu_instrs.gb
./GameBoy.exe ../../roms/dmg-acid2.gb
```
	  注意：`build/Debug` 目錄下沒有 `x64/Debug` 子資料夾；若要執行根目錄的 VS 輸出，可用：
```powershell
../../x64/Debug/GameBoy.exe ../../roms/dmg-acid2.gb
```
	  (第一個 `../../` 回到專案根，再進入 `x64/Debug`)
	- 若在 `x64\Debug`（Visual Studio 直接輸出）：ROM 相對路徑同根目錄：
```powershell
./GameBoy.exe ./roms/cpu_instrs.gb
./GameBoy.exe ./roms/dmg-acid2.gb
```

### CMake
```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Debug
build/Debug/GameBoy.exe ../../roms/cpu_instrs.gb
build/Debug/GameBoy.exe ../../roms/dmg-acid2.gb
# 或回到根目錄：
./x64/Debug/GameBoy.exe ./roms/dmg-acid2.gb
```

## 常用測試 ROM
| ROM | 目的 |
|-----|------|
| `cpu_instrs.gb` | 驗證 CPU 指令與中斷行為（已全部通過） |
| `dmg-acid2.gb` | 嚴格驗證 PPU 時序與圖像組合（已通過） |
| `tetris.gb` | 實際遊戲功能驗證 |

## 測試結果
![CPU測試結果](cpu_test.png "CPU測試全通過")
![DMG-](dmg_acid2_test.png "DMG-酸測試全通過")
![Instr Timing](instr_timing_test.png "指令週期測試")

### OAM / Sprite 測試與 acid2 使用說明
`dmg-acid2.gb` 可用來檢驗：背景/視窗組合、Sprite 排序與遮蔽、Mode2/Mode3 時序。

執行後預設會在第一個有 window 的掃描線輸出一次 OAM dump（格式：`[PPU] OAM dump (index:y x tile attr)`）。

若要反覆觀察或長期監視 OAM：
1. 在 `src\ppu.cpp` 尋找 OAM dump 的 `static bool oam_dumped` 判斷，刪除或改為計數（例如每 60 幀輸出一次）。
2. 再次建置並執行 acid2 ROM。

OAM 項目解讀：
- y/x：Sprite 左上角座標 (硬體為 y+16, x+8 的顯示偏移；程式可視需求加偏移)。
- tile：Tile 編號；8x16 模式下自動偶數對齊（顯示時會用 tile 與 tile+1 上下連接）。
- attr 位元：Priority / Y flip / X flip / Palette（僅 DMG）等；可用 bitmask 驗證。

常見檢查清單：
- acid2 中右側視窗的 Sprite 是否被背景正確遮蔽 (color0 透明 + behind_bg 規則)。
- 多於 10 個 Sprite 時僅前 10 個（OAM 順序）進入該行渲染。
- 8x16 模式的翻轉：Y flip 應交換上下半部；X flip 僅水平鏡像，不影響 tile 選擇順序。

如需正式自動化測試，可將 doctest 加入一個 OAM 驗證：執行數幀後讀 `0xFE00`~`0xFE9F` 比對期望。README 中後續將補充專用測試檔示例。



## 除錯輸出 (Debug)
目前以 `std::cout` 直接列印：
- 初始 PPU frame 設定 (LCDC/SCX/SCY/WX/WY)。
- 每幀前 12 行 window 右側 tile IDs（酸測試用）。
- OAM dump（僅第一個 window 行）。
要縮減：可在未來加入宏 `#define GB_DEBUG_PPU 0` 控制或改用 logger。

## 開發指引 / 下一步建議
1. 實作像素 FIFO：逐像素推入背景 / window，再套用 sprite 優先，解決 acid2 視窗更新時機問題。
2. 將除錯輸出改為可切換旗標，避免 I/O 對模擬速度影響。
3. 加入 PPU memory lock：Mode2 鎖 OAM、Mode3 鎖 OAM+VRAM。
4. 加入單元測試（可用 doctest 或 Catch2）驗證 palette 及 sprite behind_bg 邏輯。
5. 撰寫 APU 聲道行為測試（square / wave / noise）。

## 版權與 ROM 使用
請僅使用合法取得或公開授權的 ROM；測試 ROM 皆為社群提供的技術驗證用途。

## 歷程摘要
- 修復早期掃描線跳躍 (LY 跳 9→134)。
- 改 cycles 型別避免 uint8_t 溢位錯亂。
- 重構 PPU 模式與渲染順序；加入背景 raw color id 緩衝。
- 修正 sprite 8x16 + 翻轉邏輯與優先遮蔽。
- 修正中斷週期計算與 EI 延遲後，`cpu_instrs` 全通過。
- 實作 CPU 端 VRAM/OAM 忙碌鎖定與 PPU 專用 `ppu_read`，修復 Mode2/3 期間資料讀取；`dmg-acid2` 通過。

## 貢獻
歡迎提交 Issue / PR：可聚焦於 PPU FIFO、APU 精準化、效能或測試框架。


## 參考資料
 - https://gbdev.io/pandocs/

---
最後更新：2025-11-13