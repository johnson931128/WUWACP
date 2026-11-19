# RISC-V CPU 基礎：從 Hart、Registers 到 PC

> 本篇依據 WUWACP 遠端 `main` 的 Commit `8168a4f`（2026-11-19 查閱）撰寫。
>
> **已實作：** C++ 的 `Hart` 物件、32 個 64-bit GPR、PC、Reset、Register Read/Write 和 CPU State Unit Test。
>
> **未實作：** Machine Code Fetch、Instruction Decode/Execute、ALU、RAM/Bus、Privilege Mode、CSR、Trap、MMU 和真正的 Guest OS Boot。
>
> 相關程式：[hart.hpp](../../include/wuwacp/cpu/hart.hpp) · [hart.cpp](../../src/cpu/hart.cpp) · [cpu_state_test.cpp](../../tests/cpu_state_test.cpp)

## 1. 模擬器到底在模擬什麼？

WUWACP 的初期目標是 **Functional / Architectural Emulator**：以 Host 的 C++ 程式儲存 Guest CPU 的架構狀態，並在之後逐步加入符合 RISC-V ISA 的指令行為。

```text
Host: Windows + C++ program (wuwacp.exe)
                 |
                 v
       Software model of RV64 Hart
       +--------------------------+
       | 32 integer GPRs: x0-x31 |
       | Program Counter: PC      |
       +--------------------------+
                 |
            (not yet)
                 v
       Fetch / Decode / Execute
                 |
            (not yet)
                 v
      Guest RAM / MMIO / WUWAOS
```

目前 WUWACP **不是**一台可啟動的 RISC-V 虛擬機，也沒有時脈訊號、實體 ALU 或真實 Pipeline。雖然 `Hart` 的結構對應 CPU 架構，但現階段功能僅限於保存與管理狀態。

### 三個容易混淆的名詞

| 名稱 | 含義 | 目前 WUWACP |
| --- | --- | --- |
| ISA（Instruction Set Architecture） | 軟體可觀察的指令、暫存器與狀態規則 | 參考規格；尚未有執行器 |
| Microarchitecture | 以 Pipeline、Cache、ALU、Branch Predictor 等方式實現 ISA | 不在初期實作範圍 |
| Hart（Hardware Thread） | 具備自身架構執行狀態的 RISC-V 硬體執行單元 | 以 `Hart` 類別表示單一 Hart |

**Hart 不一定等於實體 CPU Core**。未來一個模擬平台可能有多個 Hart；目前只建立單一物件的基礎設計。

## 2. RV64：XLEN 與整數暫存器

RISC-V 基礎整數 ISA 定義 32 個整數暫存器 `x0..x31`，另有 Program Counter（PC）。`XLEN` 代表架構整數暫存器的位元寬度：

- `RV32` → `XLEN = 32`
- `RV64` → `XLEN = 64`

WUWACP 以 RV64 為主，因此程式中使用：

```cpp
std::array<std::uint64_t, 32> registers_{};
std::uint64_t pc_ = 0;
```

這是 **CPU architectural state 的一部分**，不是 Guest RAM。暫存器在 C++ 物件裡的儲存位置，是 Host 程式內存；未來的 Guest RAM 會是另一個獨立模組。

### 為什麼是 32 個，卻只有 31 個可寫？

`x0` 永遠是 0，寫入時必須忽略。可實際保存一般整數結果的暫存器是 `x1..x31`。

```text
x0  = 0               <- fixed architectural value
x1  = 64-bit value
x2  = 64-bit value
...
x31 = 64-bit value

PC  = current instruction address (separate state)
```

在 C++ 實作中保留 `registers_[0]` 的儲存格，但對外寫入介面阻止變更：

```cpp
void Hart::write_register(unsigned index, std::uint64_t value) {
    if (index == 0) {
        return;
    }
    registers_.at(index) = value;
}
```

**區分**：C++ 陣列裡有第 0 格，不代表模擬器允許 Guest 變更 `x0`。架構保證由 API 的行為來維持。

## 3. Register 名稱與 ABI 約定

RISC-V 的整數暫存器除了 `x0`、`x1` 這些架構名稱，也常見 ABI（Application Binary Interface）名稱。以下只是**軟體呼叫慣例**，不代表目前 `Hart` 具有 Function Call 或 Stack 功能。

| 架構名稱 | 常見 ABI 名稱 | 慣例用途 |
| --- | --- | --- |
| `x0` | `zero` | 固定 0 |
| `x1` | `ra` | Return Address |
| `x2` | `sp` | Stack Pointer |
| `x3` | `gp` | Global Pointer |
| `x4` | `tp` | Thread Pointer |
| `x5..x7` | `t0..t2` | Temporary |
| `x8..x9` | `s0..s1` | Saved Registers |
| `x10..x17` | `a0..a7` | Arguments / Return Values |
| `x18..x27` | `s2..s11` | Saved Registers |
| `x28..x31` | `t3..t6` | Temporary |

例如，`x2` 稱作 `sp` 是因為 ABI 約定它用於 Stack Pointer；CPU 並沒有一個與 `x2` 分開的通用整數 Stack Pointer 暫存器。

## 4. PC：Program Counter 的意義

PC 保存**目前指令的位址**，而不是儲存指令本體。實際指令要由未來的 Memory Fetch 取得。

```text
PC = 0x80000000
        |
        v
Guest physical/virtual memory  (future implementation)
        |
        v
Instruction bytes
        |
        v
Decoder (future implementation)
```

目前的 Getter 是：

```cpp
std::uint64_t Hart::pc() const noexcept {
    return pc_;
}
```

而 `reset()` 接受 PC 起始值：

```cpp
void Hart::reset(std::uint64_t reset_pc) noexcept {
    registers_.fill(0);
    pc_ = reset_pc;
}
```

這段程式**不會**檢查該位址是否有可執行程式，也不會自動做 `PC += 4`。它只設值，沒有執行任何 Guest 指令。

### 為什麼測試出現兩個不同的 PC？

| 測試值 | 本階段用途 | 平台相關背景 |
| --- | --- | --- |
| `0x80000000` | 第一輪 Reset 的指定 PC | 規劃中 QEMU `virt` RAM 起始位置 |
| `0x80200000` | 第二輪 Reset 的指定 PC | WUWAOS Kernel 的既有入口位址 |

**這兩個位址現在都只是傳給 `reset()` 的參數。** 虛擬 RAM、ELF Loader 和指令 Fetch 都還未建立，不能把測試中的地址常數誤認為已完成的 Boot 流程。

## 5. Reset：模擬器 API 與架構重置的區別

單元測試目前會：

```cpp
wuwacp::Hart cpu;

cpu.reset(0x80000000ULL);
cpu.write_register(1, 123);
cpu.write_register(31, 456);

cpu.reset(0x80200000ULL);
```

第二次 `reset()` 會清零 `x0..x31`，並讓 PC 變成 `0x80200000`。

這是 **WUWACP 自行設計的測試/初始化 API**；不代表 RISC-V 規格要求真實硬體 Reset 將所有 GPR 清為 0。後續支援真實 Firmware Boot 時，還須另外定義 Reset Vector、初始特權狀態、CSR 和設備狀態。

因此應區分：

- **現在**：`Hart::reset(reset_pc)` 保證 C++ 物件進入可預測狀態。
- **未來**：完整的 Machine Reset 必須遵循選定虛擬平台與 RISC-V Privileged Architecture 契約。

## 6. 將現有測試映射到 CPU 的規格

`tests/cpu_state_test.cpp` 現在檢查五個案例：

| Test | 操作 | 必須成立的規則 |
| --- | --- | --- |
| 1. Reset | `reset(0x80000000)` | PC 正確、所有 GPR 為 0 |
| 2. Read/Write | `x1=123`、`x31=456` | 寫入後讀取相同值 |
| 3. `x0` Invariant | 嘗試寫入 `x0=999` | 讀取仍是 0 |
| 4. Reset Again | `reset(0x80200000)` | PC 更新，先前寫入的 GPR 清零 |
| 5. Invalid Index | 嘗試寫入 `x32` | Host C++ 拋出 `std::out_of_range` |

測試中 `x32` 是錯誤的 API 索引，並非某條已執行 RISC-V 指令；因此產生的是 Host 開發時的例外，而不是 Guest Illegal Instruction Trap。

這種測試策略很重要：未來要讓 OS 正確啟動，需要先保證最基本的 CPU State 行為完全可靠。

## 7. 從狀態模型到真正的 CPU（概念預習，尚未實作）

真正的 Interpreter 至少需要以下步驟：

```text
1. Fetch    : 依 PC 從 Guest Memory 讀取 Machine Code
2. Decode   : 解讀 Opcode、rd、rs1、rs2、Immediate
3. Execute  : 執行 RISC-V 定義的操作
4. Commit   : 更新 Registers、PC 或發生 Trap
5. Repeat   : 取得下一條指令
```

接下來的學習示例可以採用 `ADDI`：

```asm
addi x1, x0, 5
```

規格上的效果：

```text
read x0 -> 0
sign-extend immediate 5
add -> 5
write x1 -> 5
```

這條指令可用 32-bit I-Type 格式編碼，例如 `0x00500093`：

```text
bits 31..20 : imm[11:0] = 5
bits 19..15 : rs1       = x0
bits 14..12 : funct3    = 000
bits 11.. 7 : rd        = x1
bits  6.. 0 : opcode    = 0010011 (0x13)
```

`ADDI` 的 Immediate 是帶號 12-bit；在 RV64 執行時，需要符號擴展到 64 bits。對這條正常執行的 32-bit 指令，下一個順序位址是 `PC + 4`。但 RISC-V 未來若支援 C（Compressed）擴充，指令也可能只有 16 bits；若發生 Jump、Branch 或 Exception，PC 更新規則也會不同。

**再提醒一次：上述 ADDI 只是 ISA 學習示例，WUWACP 遠端程式尚無 `execute_instruction()`。**

## 8. WUWAOS 的最終 ISA 目標，不等於已支援的指令

WUWACP 規劃支援 WUWAOS 的編譯目標 `rv64imac_zicsr_zifencei`：

| 記號 | 含義 | 本專案狀態 |
| --- | --- | --- |
| RV64I | 64-bit Base Integer ISA | PLANNED |
| M | 整數 Multiply / Divide | PLANNED |
| A | Atomic Memory Operations | PLANNED |
| C | Compressed Instructions | PLANNED |
| Zicsr | CSR Access Instructions | PLANNED |
| Zifencei | Instruction-Fetch Fence | PLANNED |

這不意味著已完成 FPU、MMU、S-mode、SBI 或 VirtIO。這些各有不同的擴充/特權架構與平台契約，必須逐階段實作與驗證。

## 9. 自我檢查

1. 為什麼 `std::array<std::uint64_t, 32>` 不代表有 32 個可自由寫入的整數暫存器？
2. `reset(0x80200000)` 是否等於把 WUWAOS 的 ELF 載入記憶體？
3. 為什麼 `pc()` 現在不會自動加 4？
4. `x2` 被叫作 `sp` 是 ISA 規定它必須用於 Stack，還是 ABI 習慣？
5. `write_register(32, 1)` 會造成 Guest Trap 嗎？
6. 為什麼不能把「CMake Build 成功」寫成「RISC-V Emulator 可執行」？

答案：`x0` 固定 0；不等於；還沒有執行指令；ABI 約定；目前只會拋出 Host 例外；因為 Host 執行檔與 Guest ISA 模擬功能是不同層級。

## 10. 本章完成後的合理下一步

- 深入學習 C++ 的位元運算：`&`、`|`、`^`、`<<`、`>>`。
- 了解 RISC-V 的 I-Type、Opcode、`rs1`、`rd`、Immediate。
- 實作**第一個**可測試的 `ADDI`，再逐步加入 Fetch 與 Decoder；不在本章假裝已經完成。

### 參考資料

- [RISC-V Base Integer ISA](https://docs.riscv.org/reference/isa/unpriv/rv32.html) — GPR、x0、PC、指令格式
- [RISC-V RV64I Specification](https://docs.riscv.org/reference/isa/unpriv/rv64.html) — RV64 的 XLEN 與整數行為
- [RISC-V Specifications Library](https://docs.riscv.org/)
- [WUWACP Roadmap](../ROADMAP.md)
- [WUWACP Hart Code](../../src/cpu/hart.cpp)
