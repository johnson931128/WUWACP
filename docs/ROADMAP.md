# WUWACP — Implementation Roadmap

> **Everything below is PLANNED as of 2026-11-16. No implementation, compilation, boot, or test has been run in WUWACP.** Each phase is gated by observed tests, not dates or optimistic progress.

## Definition of done for every phase

- The requested behavior exists in source and has a reproducible test.
- Build/test commands, environment, PASS/FAIL/NOT_RUN, and relevant output are recorded.
- New functionality does not break earlier accepted behavior.
- README and Architecture distinguish **implemented**, **verified**, and **planned**.
- One phase must not silently grow into the next.

## Phase 0 — Foundation and QEMU baseline

- Define the platform contract, initial ISA extensions, guest memory map, virtual-time model and direct-boot API.
- Establish a C++20/CMake/Ninja/CTest project (not yet present) and a documented Linux development command.
- Save an external or explicitly versioned baseline of WUWAOS running on QEMU `virt`, with QEMU version, exact command, ELF, DTB and UART output.
- Decide test fixture organization and a consistent CPU architectural trace format.

**Gate:** project builds and at least one real unit test executes; the QEMU baseline is reproducible.

## Phase 1 — CPU Interpreter

- CPU state: x0–x31, PC, RV64; little-endian instruction fetch.
- RV64I: integer, shifts, control flow, loads/stores, RV64 W variants.
- M extension; A extension including LR/SC and AMO (single-Hart first).
- C extension and proper 16/32-bit PC stepping; Zicsr / Zifencei.
- Illegal instructions and arithmetic corner cases; no silently skipped opcode.

**Gate:** focused ISA suites and known register/memory traces match the reference.

## Phase 2 — Physical Bus / RAM / UART / ELF

- Bounds-checked Guest RAM, address regions, MMIO dispatch, guest access widths.
- ELF64 PT_LOAD loader with segment validation, BSS zero-fill and entry point.
- 16550 UART's required register behavior with terminal output/input.
- Minimal machine CLI and small hand-authored / cross-compiled bare-metal fixture.

**Gate:** a standalone RV64 ELF prints `Hello World` from Guest instructions, not a host-side shortcut.

## Phase 3 — Privileged ISA and direct-boot contract

- M/S/U state; important status/interrupt/delegation CSRs.
- ecall, ebreak, trap entry, mret/sret, wfi, invalid CSR access.
- FDT construction and a0/a1 handoff for the selected direct-boot mode.
- Minimal SBI BASE/TIME implementation for S-mode, clearly isolated from later real firmware mode.

**Gate:** synthetic trap tests pass; WUWAOS reaches and executes its early entry path with observable state.

## Phase 4 — Sv39 MMU

- `satp`, three-level walk, 4KiB and superpage leaf semantics.
- User/supervisor permission, SUM/MXR, A/D, canonical addresses and page faults.
- `sfence.vma`; initially no TLB cache, so correctness is easier to verify.
- Regression tests for identity mappings, permissions and demand paging.

**Gate:** WUWAOS survives page-table activation and passes controlled fault/return tests.

## Phase 5 — Virtual clock / timer / interrupts

- Virtual time and timer registers required by the selected boot model.
- SBI TIME absolute deadlines in direct-boot mode; STIP behavior and trap delivery.
- Interrupt pending/enable, priority, delegation and wfi wake-up semantics.

**Gate:** Guest timer events can drive WUWAOS scheduling, with repeatable test evidence.

## Phase 6 — PLIC and VirtIO Block (first full CLI milestone)

- PLIC priority, enable, pending, claim and complete.
- Modern VirtIO MMIO feature negotiation, split virtqueue, Guest Physical DMA.
- Block read/write/FLUSH/reset and device IRQ completion; file-backed disk image.
- WUWAOS reads WUFS, reaches a serial shell and performs non-destructive file tests.

**Gate:** `wuwaos/>` is interactive; `echo`, `ls`, `ps`, basic processes and filesystem operations work. This is the primary v0.2 milestone.

## Phase 7 — Real firmware/disk boot

- Correct M-mode reset, ROM/flash mapping, PMP, low-level timer/interrupt behavior.
- Execute unmodified OpenSBI image (no fake firmware shortcut).
- Execute the WUWAOS flash bootloader and load `/boot/kernel.elf` from a VirtIO disk.
- Compare boot logs with QEMU `make run-disk` reference.

**Gate:** repeatable Reset ROM → OpenSBI → Flash Loader → WUWAOS shell path.

## Phase 8 — Optional extensions

TLB + ASID invalidation; GDB remote protocol; snapshot/replay; VirtIO GPU/tablet/keyboard; multi-Hart/PLIC/SBI IPI; basic-block cache/JIT. Each needs a separate design and validation milestone. None is required to finish Phase 6.

## Verification matrix (to be filled with actual results)

| Area | Reference | WUWACP result | Status |
| --- | --- | --- | --- |
| RV64IMAC instruction semantics | RISC-V ISA specs/tests, QEMU | — | NOT_RUN |
| CSR/traps/privilege | Privileged ISA, QEMU | — | NOT_RUN |
| Sv39/page faults | Privileged ISA, WUWAOS tests | — | NOT_RUN |
| UART/MMIO | QEMU `virt`, WUWAOS UART driver | — | NOT_RUN |
| Timer/PLIC | SBI + QEMU `virt` | — | NOT_RUN |
| VirtIO block | VirtIO spec + WUWAOS driver | — | NOT_RUN |
| Direct kernel boot | WUWAOS `make run` | — | NOT_RUN |
| Firmware/disk boot | WUWAOS `make run-disk` | — | NOT_RUN |

## Next concrete work package

**Phase 0 first.** Build the minimal executable/test harness, define Guest Physical Memory APIs and CPU state, capture a stable QEMU baseline, then start RV64I. Do not jump straight into WUWAOS boot and patch symptoms until it appears to work.
