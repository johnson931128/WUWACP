# WUWACP

![Language](https://img.shields.io/badge/language-C%2B%2B20-00599C?logo=cplusplus&logoColor=white)
![Architecture](https://img.shields.io/badge/guest-RISC--V%20RV64-283272)
![Scope](https://img.shields.io/badge/scope-full--system%20emulator-555)
![Status](https://img.shields.io/badge/status-planning-orange)

**WUWACP (WUWA Computing Platform)** is a from-scratch, C++20 RISC-V full-system emulator project. Its first long-term compatibility target is [WUWAOS](https://github.com/johnson931128/WUWAOS), an educational RV64 operating system currently run on QEMU's `virt` machine.

> **Status: repository foundation / design stage.** No CPU interpreter, bootable virtual machine, or working build is implemented yet. Everything under “Planned architecture” and “Roadmap” describes intended work, not completed features.

中文簡介：WUWACP 計畫以 C++20 從零實作 RISC-V 64-bit 全系統模擬器，逐步支援 CPU、特權模式、Sv39 MMU、記憶體映射裝置與 VirtIO，最終在不修改 WUWAOS 核心的情況下啟動系統。初期以正確性、可觀測性與可測試性為優先，而非執行速度。

## Goals

- Emulate the **architectural behavior** of an RV64 computer, not gate-level hardware or cycle-accurate microarchitecture.
- Begin with an instruction-by-instruction interpreter; treat JIT and performance optimizations as later work.
- Support the WUWAOS toolchain's `rv64imac_zicsr_zifencei` target, required privileged ISA behavior, and Sv39 virtual memory.
- Reproduce the **relevant** QEMU `virt` platform contracts: guest physical address map, UART, timers, interrupt controller, device tree, and VirtIO MMIO.
- First boot a self-contained bare-metal program, then reach a WUWAOS UART shell, then support its existing OpenSBI/flash/disk boot sequence.
- Make correctness observable through architectural trace logs, isolated tests, and comparison against QEMU.

## Non-goals for the first release

Linux compatibility; multi-hart SMP; cycle-accurate pipelines and caches; dynamic binary translation; GPU, keyboard, tablet, or graphical desktop; universal compatibility with every `virt` device. These may be considered later, once the single-hart CLI path works.

## Planned architecture

```text
              Guest: WUWAOS / bare-metal RV64 ELF
                               |
                               v
+--------------------------------------------------------------+
|                     WUWACP (host C++20)                      |
|                                                              |
|  +---------------------+    +----------------------------+   |
|  | RV64 Hart           |    | Physical Bus               |   |
|  | GPR / PC            |<-->| RAM / ROM                  |   |
|  | Decode / Execute    |    | MMIO mapping               |   |
|  | CSR / Trap          |    | DMA access                 |   |
|  | Sv39 MMU            |    +----------------------------+   |
|  +---------------------+                  |                  |
|                                           v                  |
|                             +----------------------------+   |
|                             | MMIO Devices               |   |
|                             | UART / Timer / PLIC        |   |
|                             | VirtIO MMIO + Block Disk   |   |
|                             +----------------------------+   |
+--------------------------------------------------------------+
                               |
                               v
             Host: Linux (first) / Windows (later)
```

The CPU must access devices through the physical bus rather than calling guest-specific OS functions. VirtIO must model guest-visible queues and DMA; it must not bypass the guest driver with host-side syscall forwarding. Guest faults and interrupts change guest architectural state; they must not crash the host.

See [Architecture](docs/ARCHITECTURE.md) for module boundaries, boot contracts, and compatibility assumptions.

## Development roadmap

| Phase | Target | Acceptance gate |
| --- | --- | --- |
| 0 | Project foundation | Documents, build/test design, QEMU baseline recorded |
| 1 | RV64IMAC interpreter | Small instruction tests pass against a reference |
| 2 | RAM, MMIO bus, ELF loader, UART | Bare-metal ELF prints over serial |
| 3 | M/S/U privilege, CSRs, traps, minimal SBI | Controlled privilege transitions and trap tests pass |
| 4 | Sv39 translation and page faults | Page-table and user/kernel isolation tests pass |
| 5 | Timer and interrupts | Guest timer traps drive scheduling |
| 6 | PLIC and VirtIO block | WUWAOS reaches an interactive UART shell |
| 7 | Reset ROM, real OpenSBI and flash boot | Existing WUWAOS disk-boot path runs |
| 8 | TLB, debugger, optional devices and optimization | Independent verification for each additional feature |

See [Roadmap](docs/ROADMAP.md) for detailed prerequisites and test criteria. Phases are targets, not implementation claims.

## WUWAOS compatibility target

The current WUWAOS project builds RV64 code with `-march=rv64imac_zicsr_zifencei`, uses QEMU `-machine virt -m 128M`, and boots its kernel at `0x80200000`. Its CLI path uses an 16550-compatible UART, Sv39, SBI time services, PLIC and VirtIO block storage. It also has optional VirtIO graphics/input, which are explicitly deferred.

Two distinct boot modes are planned:

1. **Direct kernel boot (development shortcut):** load the WUWAOS ELF, set up a compatible S-mode entry environment and minimal SBI behavior, then execute the kernel. This is *not* full firmware emulation.
2. **Firmware/disk boot (full-system target):** start in M-mode, execute real OpenSBI, hand off to the flash bootloader, read `/boot/kernel.elf` through VirtIO, and enter WUWAOS.

The concrete device map and device-tree layout will be validated against the QEMU version used for the baseline; an approximate map is not considered proof of compatibility.

## Building and running

**Not available yet.** The repository intentionally contains documentation and planning files only. There is no `CMakeLists.txt`, working `minirv` executable, runnable firmware, or validated build command at this stage.

The proposed stack is C++20 + CMake + Ninja + CTest on Linux first. Once Phase 0 introduces an actual build, this section will be replaced with commands that have been executed and checked.

## Verification approach

- Instruction/CSR/MMU unit tests must include boundary values and failure paths.
- Integration tests must report observable guest state, not merely “no crash.”
- QEMU provides a behavioral reference, not an excuse to copy its source code.
- Logs must distinguish **PASS**, **FAIL**, and **NOT_RUN**. Do not describe planned, simulated, or previously observed results as current test evidence.
- Commit small, reviewable changes with clear test results.

## Project documents

- [AGENTS.md](AGENTS.md) — repository-specific rules for Codex and other coding agents.
- [Architecture](docs/ARCHITECTURE.md) — planned hardware/software contracts.
- [Roadmap](docs/ROADMAP.md) — phase gates and acceptance criteria.
- [Repository metadata](docs/REPOSITORY_METADATA.md) — suggested GitHub description/topics.
- [WUWAOS](https://github.com/johnson931128/WUWAOS) — initial guest OS.
- [RISC-V specifications](https://docs.riscv.org/) · [QEMU `virt` documentation](https://www.qemu.org/docs/master/system/riscv/virt.html) · [RISC-V SBI specification](https://github.com/riscv-non-isa/riscv-sbi-doc)

## License

A license has not yet been selected for WUWACP. This repository is not implicitly licensed under WUWAOS's license.
