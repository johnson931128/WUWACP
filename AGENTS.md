# WUWACP — Agent Instructions

本檔案適用於整個 WUWACP repository 的 Coding Agents（Codex、Claude Code 等）。這是**從零開發的 C++20 RISC-V RV64 Full-System Emulator**，以 WUWAOS 相容性為核心驗收目標。文件中的 roadmap 是規劃，**不代表功能已實作**。

## 不可妥協的工程原則

- **架構正確性 > 效能 > 額外功能**。先完成 RV64 ISA interpreter，再處理 privilege / MMU / devices；在驗證前不引入 JIT、Pipeline、Cycle-accurate 模擬或不必要的 framework。
- **嚴格維持 Host 與 Guest 邊界**：CPU 只透過 Bus / MMU 操作 Guest Memory；UART、PLIC、Timer、VirtIO 透過裝置介面與 Guest 溝通。禁止為了讓 WUWAOS 開機而直接呼叫其 Kernel 內部函式、代替 Guest 執行 syscall，或跳過 Guest Driver。
- **符合 RISC-V 規格**：x0 恆為零、指令長度/PC、符號擴展、整數邊界、CSR 權限、Trap 與 `xRET`、Sv39 PTE/權限/A-D、`sfence.vma`、中斷/委派都需要可重現測試。不清楚的 ISA 細節先查原始規格，不用猜測或簡化成錯誤行為。
- **不依賴 WUWAOS 專用作弊路徑**：維持通用 RISC-V 架構層；WUWAOS 是第一個 Guest 與驗收目標，不是 CPU 的硬編碼特例。
- **誠實狀態**：不把計畫寫成已完成，不把 QEMU 或先前版本的成功當成本次 WUWACP 實測。分別註明 PASS / FAIL / NOT_RUN、使用的指令、版本與限制。

## 工作流程

1. 先了解任務範圍、相關檔案和已存在的測試；**只閱讀與當次修改有關的文件**，避免為小變更讀完整倉庫。
2. CPU / MMU / Trap / Boot / Device 邊界變更時閱讀 `docs/ARCHITECTURE.md`；排程和新階段工作閱讀 `docs/ROADMAP.md`。與 WUWAOS 相容性相關的設計，確認其現有程式碼與 QEMU 啟動參數。
3. 重大功能、跨模組重構或影響 Guest 可觀察行為的變更，先寫簡短計畫：目標、非目標、相依、實作步驟、驗證；若缺少關鍵架構決策或需擴大已授權範圍，列出選項與取捨，取得明確方向後再處理。
4. 一次只完成一個可驗收里程碑。優先最小、可審查的改動；禁止無關重命名、大規模格式化、擅自更改模擬平台契約。
5. 修改完成需實際執行可用的建置與相關測試；若尚無建置系統、工具或硬體映像，明確記錄 NOT_RUN 和原因，**不可補寫虛構的通過結果**。
6. 報告改了什麼、實測結果、尚未驗證的部分，以及下一個具體工作。避免無意義的進度宣稱。

## 程式設計規範（當原始碼開始出現後適用）

- 預設 C++20、CMake、Ninja、CTest；Linux 作為主要開發/驗證環境，保留跨平台介面，不引入無必要的相依套件。
- 類別職責明確：`Hart` / Decode / Execute、CSR / Trap、MMU、Bus、RAM、MMIO Device、Machine、Loader、Clock 分離。使用 RAII、固定寬度整數、`enum class`、明確的存取寬度與端序。
- 偏好易讀的 OOP 與小函式；避免 magic number。常數（地址、bit mask、exception code、device registers）集中管理並註記參照規格。
- Host 輸入與 Guest 記憶體指標要驗證範圍、對齊、溢位及位址回繞。Guest fault 回報為模擬器狀態 / Trap，不應造成 Host undefined behavior。
- Bus 對 MMIO 的載入/儲存明確處理訪問大小及對齊；Guest DMA 經過 Guest Physical Memory 接口，不直接假設 Host pointer 有效。
- 保持決定性：虛擬時鐘與裝置事件有明確時間模型；測試中不依賴無界 sleep 或不可重現的 Host 排程。
- 單 Hart、Interpreter、UART CLI 優先。未經需求確認不新增多 Hart、TLB 快取、GPU、JIT、網路或 GUI。

## 測試與相容性

- 對每個指令族提供單元與邊界案例；同時涵蓋非法指令、權限錯誤、缺頁、裝置邊界與中斷。
- 參考 QEMU 與 RISC-V ISA Tests 的**架構行為**，不以 QEMU 私有實作為設計依據。對比 PC、GPR、CSR、Memory Side Effects、Trap 原因等可觀察狀態。
- 每個重大模組都要有獨立測試；WUWACP 能執行 Bare-metal Hello World ≠ 已能啟動 WUWAOS，能直接載入 Kernel ≠ 已支援真實 OpenSBI Firmware Boot。
- 測試失敗應先縮小最小可重現範例；不得透過忽略例外、修改 Guest、硬編碼輸出或無條件 PASS 來迴避。
- 不修改 WUWAOS 來配合錯誤的模擬器行為；若確認是 Guest 本身的問題，另行記錄證據並明確區分。

## 文件、溝通與 Git

- 原始碼、符號、Commit Message 使用精準英文；技術說明可以使用繁體中文，保留英文專有名詞，第一次出現時解釋用途。Markdown 維持清楚層級，避免含糊語句與過度裝飾。
- README 用於公開定位與實際可使用的指令；Roadmap 用於未來計畫；Architecture 用於設備契約。功能進度變更時，同步更新相關文件且清楚分開 Planned / Implemented / Verified。
- **預設只在 `main` 作業**。未明確要求時，不主動建立 Branch、PR 或 Worktree；不得 Force Push、覆蓋既有工作、刪除資料或更動授權/重大架構。Commit / Push / Release 以當次任務授權為準。
- 可自行處理已清楚授權範圍內的例行工程細節；對重大選擇、新依賴、破壞性變更以及超出需求的行為，先說明取捨再確認，不以沉默作為同意。
- 使用 Conventional Commits（例如 `feat(cpu): ...`、`test(mmu): ...`、`docs: ...`）。提交前檢查 diff、未追蹤檔案與可重現的測試結果。

## 參考依據

- [RISC-V Specifications](https://docs.riscv.org/)
- [RISC-V SBI Specification](https://github.com/riscv-non-isa/riscv-sbi-doc)
- [QEMU RISC-V virt Machine](https://www.qemu.org/docs/master/system/riscv/virt.html)
- [WUWAOS Source](https://github.com/johnson931128/WUWAOS)

隨專案實際結構成熟，才視需要在特定子目錄補上更窄範圍的 `AGENTS.md`；不要複製整份通用規範。
