# C++20 語法筆記：從 WUWACP 的 Hart 開始

> 範圍：WUWACP 遠端 `main`，基準 Commit `8168a4f`（2026-11-19 查閱）。本章解釋**已存在**的 C++ 程式碼，不把後續構想當作現有功能。
>
> 對照原始碼：[hart.hpp](../../include/wuwacp/cpu/hart.hpp) · [hart.cpp](../../src/cpu/hart.cpp) · [cpu_state_test.cpp](../../tests/cpu_state_test.cpp)
>
> 閱讀順序：**本章 → [建置與測試指令](02-build-and-test-commands.md) → [RISC-V CPU 基礎](03-riscv-cpu-basics.md)**

## 1. 目前程式在做什麼？

專案以 C++20 描述一個 RISC-V Hart（Hardware Thread）的**部分架構狀態**：32 個 64-bit General Purpose Registers（GPR），以及一個 64-bit Program Counter（PC）。

`Hart` 目前具備 `reset()`、`read_register()`、`write_register()`、`pc()` 四種操作，**還沒有** Fetch、Decode、Execute、RAM、CSR 或 Trap。

程式入口 `src/main.cpp` 只印出初始化訊息，沒有建立 Hart；CPU 的行為目前由單獨的測試程式檢查。

## 2. Header 和 Source：`.hpp`、`.cpp`

這兩個檔案扮演不同角色：

| 檔案 | 現在的用途 | 重點 |
| --- | --- | --- |
| `include/wuwacp/cpu/hart.hpp` | 宣告類別與介面 | 呼叫者知道有哪些方法和資料型別 |
| `src/cpu/hart.cpp` | 定義方法的實際行為 | 實作讀寫、清除和狀態更新 |
| `tests/cpu_state_test.cpp` | 另一個可執行程式 | 驗證已宣告介面的行為 |

例如，Header 中：

```cpp
class Hart {
public:
    void reset(std::uint64_t reset_pc) noexcept;
};
```

Source 中則有對應定義：

```cpp
void Hart::reset(std::uint64_t reset_pc) noexcept {
    registers_.fill(0);
    pc_ = reset_pc;
}
```

`Hart::reset` 中的 `::` 是 Scope Resolution Operator（範圍解析運算子），表明此函式屬於 `Hart` 類別。

## 3. `#pragma once`、`#include` 與 Namespace

```cpp
#pragma once

#include <array>
#include <cstdint>

namespace wuwacp {
    // Hart class...
}
```

- `#pragma once`：防止同一個 Header 在單一編譯單元內重複展開；為廣泛支援的編譯器擴充，不是 C++ 標準關鍵字。
- `#include <array>`：取得固定容量容器 `std::array`。
- `#include <cstdint>`：取得固定寬度整數型別，例如 `std::uint64_t`。
- `namespace wuwacp`：避免大型專案的名稱衝突。
- `std::`：C++ 標準函式庫所使用的命名空間前綴。

專案採用**明確的命名空間**，不需要在 Header 加入 `using namespace std;`。

## 4. Class、存取修飾詞與 Encapsulation

原始碼核心結構：

```cpp
class Hart {
public:
    void reset(std::uint64_t reset_pc) noexcept;
    std::uint64_t read_register(unsigned index) const;
    void write_register(unsigned index, std::uint64_t value);
    std::uint64_t pc() const noexcept;

private:
    std::array<std::uint64_t, 32> registers_{};
    std::uint64_t pc_ = 0;
};
```

- `class`：宣告類別；`public` 公開操作介面，`private` 保護內部狀態。
- `std::array<std::uint64_t, 32>`：**32 個元素，每個元素 64 bits**，容量編譯期固定。
- `registers_{}`：使用空的大括號初始化，所有整數元素成為 0。
- `pc_ = 0`：宣告時提供初始值。
- `;`：C++ 類別定義結尾必須有分號。
- 末尾的 `_` 是常見的私有成員命名習慣，不是語法要求。

**為什麼不把 `registers_` 設為 public？** 若任意程式碼可直接指定 `registers_[0] = 999`，就會破壞 RISC-V 的 `x0 = 0` 規則。所有正常寫入都經由 `write_register()`，才能維持不變量（Invariant）。

## 5. `std::uint64_t`、`unsigned` 與數字 Literal

```cpp
std::uint64_t pc_ = 0;
void write_register(unsigned index, std::uint64_t value);
```

- `std::uint64_t`：無號 64-bit 整數；RV64 的暫存器是 64 bits，因此適合表示**原始位元狀態**。
- `unsigned`：在此作為暫存器索引型別。**不代表**索引必然合法，合法範圍仍是 `0..31`。
- `0x80000000ULL`：十六進位 Literal；`ULL` 表示 `unsigned long long`。
- `0x80200000ULL`：測試使用的另一個 PC 值；不是 CPU 自動知道的重置位址。

**進階提醒**：`std::uint64_t` 只是承載位元。日後模擬帶號運算時仍須遵循 ISA 的 Sign Extension、Overflow 等規則，不能直接以 Host 的 signed overflow 來代表 Guest CPU 行為。

## 6. `const`、`noexcept` 與 `[[nodiscard]]`

Header 目前使用：

```cpp
[[nodiscard]]
std::uint64_t read_register(unsigned index) const;

[[nodiscard]]
std::uint64_t pc() const noexcept;
```

| 語法 | 含意 | 在 Hart 的用途 |
| --- | --- | --- |
| `const`（方法尾端） | 不允許此方法透過一般成員存取修改物件的非 `mutable` 狀態 | 讀取 Register 和 PC |
| `noexcept` | 宣告函式不應讓例外離開；若例外真的逸出，會終止程式 | `reset()`、`pc()` |
| `[[nodiscard]]` | 允許編譯器對忽略回傳值發出診斷 | 提醒讀取結果應被使用 |

**不要混淆**：`const` 是物件狀態的操作約束；`noexcept` 是例外傳播契約；`[[nodiscard]]` 是使用結果的提醒。三者用途不同。

## 7. `std::array::fill()`、`at()` 與 Index Checking

```cpp
void Hart::reset(std::uint64_t reset_pc) noexcept {
    registers_.fill(0);
    pc_ = reset_pc;
}
```

`fill(0)` 會將陣列中**全部 32 個暫存器**設為 0，而 `pc_ = reset_pc` 則保留呼叫者傳入的起始執行位址。

```cpp
std::uint64_t Hart::read_register(unsigned index) const {
    return registers_.at(index);
}
```

`at(index)` 帶有 Bounds Checking（邊界檢查）。當 `index >= 32` 時，會拋出 `std::out_of_range`，而不是像未檢查的陣列存取那樣可能造成 Undefined Behavior。

```cpp
void Hart::write_register(unsigned index, std::uint64_t value) {
    if (index == 0) {
        return;
    }
    registers_.at(index) = value;
}
```

逐行理解：

1. `if (index == 0)`：檢查是否要寫入 `x0`。
2. `return;`：提早離開 `void` 函式，不進行任何寫入。
3. `registers_.at(index) = value;`：只有非 `x0` 的合法索引才更新。

這段程式同時體現兩個不同層次：

- **Guest ISA 規則**：`x0` 永遠讀到 0。
- **Host C++ 防錯**：如果模擬器呼叫不存在的 `x32`，在開發時拋出例外協助定位 Bug。

此時的 `std::out_of_range` **不是** RISC-V Guest Trap；真正的 Guest Exception 處理尚未實作。

## 8. 單元測試的 `if`、`for`、`try/catch`

```cpp
for (unsigned i = 0; i < 32; ++i) {
    if (cpu.read_register(i) != 0) {
        std::cerr << "[FAIL] Register reset\n";
        return 1;
    }
}
```

- `for (初始值; 條件; 更新)`：依序檢查 `x0..x31`。
- `++i`：將 `i` 加 1。
- `!=`：不等於。
- `std::cerr`：標準錯誤輸出。
- `return 1`：測試程式以非 0 Exit Code 代表失敗。

測試中的例外檢查：

```cpp
try {
    cpu.write_register(32, 1);
    std::cerr << "[FAIL] Invalid index\n";
    return 1;
} catch (const std::out_of_range&) {
    // Expected
}
```

`try` 區塊執行可能拋出例外的程式。若因索引 32 導致 `std::out_of_range`，由 `catch` 接住；這裡刻意不讀取例外物件，因為只需證明「有拋出正確型別」。

測試全數通過才會：

```cpp
std::cout << "[PASS] CPU state tests\n";
return 0;
```

`std::cout` 是標準輸出；作業系統 Exit Code `0` 慣例上表示成功。CTest 以測試程式退出狀態作為主要的 PASS/FAIL 判斷依據，**不是只要印出 `[PASS]` 就算通過**。

## 9. 自主練習（不用修改正式程式碼）

1. `Hart cpu;` 建構之後、呼叫 `reset()` 之前，`pc()` 會回傳什麼？從成員初始化推導。
2. `write_register(0, 999)` 為何不需要執行 `at(0)`？
3. `read_register(32)` 與 `write_register(32, 1)` 會有什麼反應？
4. 取消 `registers_` 的 `private` 封裝，對 RISC-V 的不變量會有什麼風險？
5. 如果一個方法同時有 `const noexcept`，分別代表什麼？

答案提示：0；提早返回；`std::out_of_range`；可能讓 `x0` 被直接覆寫；「不改變正常成員狀態」與「不允許例外逸出」。

## 10. 後續章節的界線

| 主題 | 遠端實作狀態 |
| --- | --- |
| C++ 類別、Namespace、`std::array`、讀寫/Reset | **IMPLEMENTED** |
| CMake + CTest 的 CPU State 測試程式 | **IMPLEMENTED** |
| Decode / Execute、`ADDI` | **NOT IMPLEMENTED** |
| Bus、RAM、ELF Loader、CSR、MMU | **NOT IMPLEMENTED** |

本章可以作為下一步撰寫 Decoder 的 C++ 語法基礎，但不預先宣稱 Decoder 已完成。

### 參考資料

- [C++ `std::array::at` — cppreference](https://en.cppreference.com/w/cpp/container/array/at)
- [C++ `[[nodiscard]]` — cppreference](https://en.cppreference.com/w/cpp/language/attributes/nodiscard)
- [C++ `noexcept` — cppreference](https://en.cppreference.com/w/cpp/language/noexcept_spec)
- [WUWACP Hart Header](../../include/wuwacp/cpu/hart.hpp)
