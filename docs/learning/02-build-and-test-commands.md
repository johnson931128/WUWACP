# WUWACP 指令筆記：PowerShell、CMake、Ninja、G++、CTest、Git

> 適用環境：Windows PowerShell、VS Code、MinGW-w64 GCC、CMake 與 Ninja。
>
> 遠端基準 Commit：`8168a4f`。依據目前 [CMakeLists.txt](../../CMakeLists.txt)，正式定義的 Test 只有 `cpu_state`；尚無 `cpu_addi` 或其他 ISA 測試。
>
> 對照學習：[C++20 語法](01-cpp20-syntax.md) · [RISC-V CPU 基礎](03-riscv-cpu-basics.md)

## 1. 先建立正確的工具分工

從原始碼到測試結果，不是由單一工具完成：

```text
CMakeLists.txt + .cpp/.hpp
           |
           v
      CMake Configure
           |
           v
    Ninja Build Rules
           |
           v
  GCC/G++ Compile + Link
           |
     +-----+-----------+
     |                 |
     v                 v
 wuwacp.exe     cpu_state_test.exe
                       |
                       v
                     CTest
                       |
                 PASS / FAIL
```

| 工具 | 角色 | 是否直接理解 RISC-V Guest 指令？ |
| --- | --- | --- |
| PowerShell | Host 終端機，執行命令與管理路徑 | 否 |
| CMake | 讀取 `CMakeLists.txt` 並產生建置規則 | 否 |
| Ninja | 根據規則安排需要執行的建置步驟 | 否 |
| G++ | 編譯 **Host 上的 C++ 模擬器** | 否；不是 RISC-V Cross Compiler |
| Linker | 把 Object Files / Libraries 連成執行檔 | 否 |
| CTest | 執行已註冊的測試並收集結果 | 否 |
| Git | 版本控制、比對變更、同步程式碼 | 否 |

**最重要的區分**：MinGW 的 `g++.exe` 將 WUWACP 編譯成 Windows Host 的程式；以後需要編譯 RISC-V Guest ELF 時，會使用不同的 RISC-V Cross Toolchain。不要把 Host Compiler 和 Guest ISA 混在一起。

## 2. 確認工具是否存在

在專案根目錄開啟 VS Code 的 PowerShell：

```powershell
Get-Command cmake, ninja, g++, clang++, cl -ErrorAction SilentlyContinue |
    Select-Object Name, Source

cmake --version
g++ --version
git status --short
```

- `Get-Command`：PowerShell 查詢執行檔或命令。
- `|`：Pipeline，將左側結果傳給右側。
- `Select-Object Name, Source`：只顯示指定欄位。
- `-ErrorAction SilentlyContinue`：對未找到的工具不顯示非終止性錯誤。
- `--version`：列出工具版本。
- `git status --short`：快速確認修改/未追蹤檔案。

曾提供的本機輸出為 CMake `3.30.4`、G++ `14.2.0`、MinGW 路徑 `C:\mingw64\bin`。此為**先前提供的本機記錄**，不是遠端 GitHub 能重驗的環境狀態。

## 3. Configure：CMake 產生建置規則

```powershell
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
```

逐個拆解：

| 片段 | 意義 |
| --- | --- |
| `cmake` | 啟動 CMake |
| `-S .` | Source Tree 是目前資料夾 |
| `-B build` | Build Tree 是專案中的 `build/` |
| `-G Ninja` | 指定 Ninja Generator |
| `-D...` | 設定 CMake Cache 變數 |
| `CMAKE_BUILD_TYPE=Debug` | 單一配置 Generator 產生 Debug 建置 |

首次 Configure 會進行 Compiler Detection、檢查專案定義並產生 `build.ninja` 等檔案。**Configure 本身不等於已經編譯程式。**

專案的 `CMakeLists.txt` 包含：

```cmake
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
```

代表使用 C++20、不得退回較舊標準，且不要求編譯器特有語言擴充。

## 4. Build：Ninja 呼叫 Compiler 與 Linker

```powershell
cmake --build build
```

CMake 的 `--build` 會呼叫所選 Generator 的原生建置工具（本專案是 Ninja）。對應目前專案結構：

- `src/cpu/hart.cpp` → 編入 `wuwacp_core` Library。
- `src/main.cpp` + `wuwacp_core` → `wuwacp.exe`。
- `tests/cpu_state_test.cpp` + `wuwacp_core` → `cpu_state_test.exe`。

查看更詳細的編譯與連結指令：

```powershell
cmake --build build --verbose
```

正常的 Incremental Build（增量建置）只會重建被修改影響的 Targets，並不要求每次刪除 `build/`。

## 5. Run：執行兩個不同的 EXE

```powershell
.\build\wuwacp.exe
.\build\cpu_state_test.exe
```

PowerShell 中 `.` 指定「目前目錄下」，`build` 是先前設定的建置資料夾。

目前 `wuwacp.exe` 預期輸出：

```text
WUWACP - RISC-V Full-System Emulator
Status: Initialization successful
```

它**沒有**開始執行任何 RISC-V Machine Code；目前只是 Host Console 程式。

`cpu_state_test.exe` 則會直接建立 `Hart` 並執行測試，成功時印出：

```text
[PASS] CPU state tests
```

上述為依目前程式碼推得的**預期輸出**，並非當次遠端 CI 的實測結果。

## 6. Test：用 CTest 執行已註冊測試

```powershell
ctest --test-dir build --output-on-failure
```

| 片段 | 意義 |
| --- | --- |
| `ctest` | CMake 提供的測試執行器 |
| `--test-dir build` | 尋找該 Build Tree 中註冊的測試 |
| `--output-on-failure` | 測試失敗時印出該測試程式的輸出 |

目前 CMake 中的對應內容：

```cmake
enable_testing()

add_executable(cpu_state_test
    tests/cpu_state_test.cpp
)

target_link_libraries(cpu_state_test
    PRIVATE wuwacp_core
)

add_test(
    NAME cpu_state
    COMMAND cpu_state_test
)
```

`add_executable` 產生測試程式；`target_link_libraries` 讓它連結到 CPU Core；`add_test` 才是讓 CTest 知道這個測試存在的關鍵。

其他實用指令：

```powershell
ctest --test-dir build -N
ctest --test-dir build -V
ctest --test-dir build --output-on-failure
```

- `-N`：只列出被登記的測試，不實際執行。
- `-V`：Verbose，顯示測試詳細輸出。
- CTest 根據測試的 Exit Status 判斷結果；`[PASS]` 字串只是一則人類可讀訊息。

**重要**：修改 C++ 程式後，應先 Build 再 CTest。CTest 不會自動重編最新原始碼；否則有可能測試到舊的 EXE。

## 7. 常見問題與診斷順序

| 現象 | 所在階段 | 先檢查什麼？ |
| --- | --- | --- |
| 找不到 CMake / Compiler | 環境 | `Get-Command`、PATH |
| `CMakeLists.txt` 有錯 | Configure | Target 名稱、括號、相對路徑 |
| `fatal error: ... No such file` | Compile | `#include` 與 Include Directory |
| `undefined reference` | Link | 方法有無定義、Library 有無連結 |
| CTest 顯示 Failed | Run/Test | 測試輸出、Exit Code、實際 CPU 狀態 |
| 原始碼改了但行為沒變 | Build/Test | 是否重新 `cmake --build build` |
| Ninja 與別的 Generator 衝突 | Configure | 是否在同一 `build/` 混用不同 Generator |

若要切換 Generator 或 Compiler，通常應使用**新的 Build Tree**，例如 `build-msvc/`，而不是混用舊的 CMake Cache。

## 8. Git 同步與本機操作

檢查：

```powershell
git status --short
git branch --show-current
git log -5 --oneline
git remote -v
```

安全地更新本機（執行前先看 `git status`，保護尚未提交的工作）：

```powershell
git fetch origin
git pull --ff-only origin main
```

- `git fetch`：只更新遠端追蹤資訊，不直接修改 Working Tree。
- `git pull --ff-only`：只允許 Fast-forward，避免無意建立 Merge Commit；若本機修改衝突，應先處理，不強迫覆蓋。
- GitHub Repository 遠端更新**不會**自動同步 VS Code 本機檔案。

完成修改、確認 Diff 並自行決定提交內容：

```powershell
git status --short
git diff
git add <specific-file-path>
git commit -m "docs: ..."
git push origin main
```

上面 `<specific-file-path>` 只是佔位符，須替換為實際路徑。不要照抄尖括號內容，也不建議在不清楚變更範圍時直接 `git add .`。

## 9. 開發時最實用的心智模型

```text
修改 .cpp / .hpp
    ↓
cmake --build build
    ↓
ctest --test-dir build --output-on-failure
    ↓
確認結果 + git diff
    ↓
經明確授權 / 自行決定後再 commit / push
```

第一次配置、Build Tree 變更或重要 CMake 設定修改時再執行 Configure；修改普通原始碼大多不需要每次手動重做 Configure。

### 參考資料

- [CMake CLI Manual](https://cmake.org/cmake/help/latest/manual/cmake.1.html)
- [CTest CLI Manual](https://cmake.org/cmake/help/latest/manual/ctest.1.html)
- [Ninja Manual](https://ninja-build.org/manual.html)
- [WUWACP CMakeLists.txt](../../CMakeLists.txt)
