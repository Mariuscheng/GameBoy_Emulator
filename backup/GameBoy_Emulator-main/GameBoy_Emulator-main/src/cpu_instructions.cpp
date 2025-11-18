// 指令解碼與執行相關實作，從 cpu.cpp 拆出
#include "cpu.h"
#include <iostream>

// 指令解碼與執行相關實作，預留檔案（目前主要邏輯仍在 cpu.cpp、cpu_alu.cpp、cpu_cb.cpp、cpu_rotate.cpp、cpu_bits.cpp）。
// 若未來要進一步重構，建議把 cpu.cpp 的大 switch 搬移到這裡，並保持每個領域（ALU/CB/rotate/bits）在各自檔案內。

// 例如:
// void CPU::execute_instruction(uint8_t opcode) { ... }
// void CPU::execute_cb_instruction(uint8_t opcode) { ... }
// 其他 ALU/flag/bit 操作等
