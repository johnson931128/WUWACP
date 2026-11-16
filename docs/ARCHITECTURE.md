# WUWACP — Architecture Contract

> **Status: proposed design, not implemented.** This document defines intended module boundaries and the first WUWAOS compatibility target. Specifications and actual tests take precedence over assumptions.

## 1. Scope and guest platform

- Host: C++20; Linux first, Windows portability later.
- Guest: 64-bit little-endian RISC-V; initial extension target `rv64imac_zicsr_zifencei`.
- Execution: single Hart, interpreter, architectural (functional) emulation. Not cycle-accurate.
- RAM target: 128 MiB from guest physical address `0x80000000`, matching the WUWAOS QEMU CLI setup.
- Initial OS target: [WUWAOS](https://github.com/johnson931128/WUWAOS). The device model should remain generic and spec-driven.

The actual WUWAOS build flags and boot commands must be checked when implementation begins. A fresh QEMU `virt` baseline should record machine/firmware versions, Device Tree, memory map, and serial output before claiming byte-for-byte compatibility.

## 2. Module boundaries

| Module | Owns | Must not do |
| --- | --- | --- |
| Hart | GPRs, PC, current privilege, instruction step | Know WUWAOS-specific functions or filesystem format |
| Decoder / Execute | RV64 instruction decoding and architectural changes | Read host resources directly |
| CSR / Trap | CSR access checks, exception and interrupt state transitions | Auto-save all GPRs as if hardware did it |
| MMU | Sv39 walk, page checks, access context, optional future TLB | Silently map failed translations to RAM |
| Bus | Guest physical address routing, alignment/width policy | Pretend device MMIO is ordinary RAM |
| RAM | Bounds-checked little-endian guest memory | Own virtual address translation |
| Device | MMIO registers, device-specific event/state | Bypass guest-visible protocols |
| Machine | Hart wiring, devices, reset, virtual clock, scheduling | Implement guest OS syscalls |
| Loader / Firmware | ELF/PT_LOAD and the selected boot contract | Conflate direct boot with actual firmware execution |
| Diagnostics | Optional instruction/CSR/bus/trap trace | Change guest observable behavior when enabled |

## 3. Instruction and memory path

```text
Hart.step()
  -> pending interrupts? (architectural priority / masks)
  -> fetch instruction (MMU execute translation + physical bus)
  -> decode 16-bit compressed or 32-bit instruction
  -> perform architectural operation
       -> load/store: MMU translate -> bus -> RAM/MMIO
       -> CSR: checked CSR access
       -> fault/interrupt: trap entry with original fault PC
  -> commit PC / register state or take trap
```

Requirements: x0 is hard-wired to zero; RV64 word operations sign-extend as specified; misalignment and access fault behavior must be explicit; instruction fetch must honor instruction boundaries/permissions; illegal instruction remains a Guest Trap. An initial MMU may perform a page-table walk on every access; in that case, `sfence.vma` still exists but no TLB cache needs invalidation.

## 4. Privilege and Sv39

The machine must support M/S/U privileges, their relevant CSRs, exception/interrupt delegation, `mret`/`sret`, and `wfi`. Sv39 translation needs canonical address checks, three-level traversal, leaf/superpage rules, permissions (including SUM/MXR), and A/D handling. Validate `scause`/`stval`/`sepc` and S-mode return semantics independently.

Any minimal SBI implementation provided by the **direct-boot shortcut** is a development compatibility layer and must be kept out of the real OpenSBI execution path.

## 5. Address map and devices

The target is the portion of QEMU `virt` needed by WUWAOS, not an invented board. Starting points for planning:

| Guest physical address | Intended model |
| --- | --- |
| `0x00001000` | Reset ROM (later firmware boot) |
| `0x02000000` | CLINT/timer-related platform devices |
| `0x0C000000` | PLIC |
| `0x10000000` | 16550-compatible UART |
| `0x10001000` | VirtIO MMIO slots (beginning of region) |
| `0x20000000` | Flash region (later firmware boot) |
| `0x80000000` | RAM, target 128 MiB |
| `0x80200000` | WUWAOS kernel entry **inside RAM**, not a separate device |

**Do not treat this abbreviated table as a complete authoritative map.** For every region verify range, overlap, interrupt line, device-tree `reg`, clock and actual QEMU revision. The firmware/boot platform may require additional registers and reset behavior.

The UART must model registers including the WUWAOS-used line-status bits. VirtIO MMIO must support modern (v2) negotiation, queue addresses, descriptor/avail/used rings, Guest Physical DMA, block I/O and FLUSH semantics. The PLIC must support device IRQ routing and claim/complete needed by the guest.

## 6. Two separate boot modes

### Development: direct kernel boot

1. Load an ELF64 kernel's PT_LOAD segments into Guest RAM and zero the BSS portions.
2. Build a valid Device Tree and supply the boot ABI (`a0` hart ID, `a1` DTB pointer).
3. Set a compatible S-mode entry environment and PC (WUWAOS currently `0x80200000`).
4. Provide only the minimal SBI services needed for early development.
5. Execute WUWAOS `entry.S`; verify early UART and traps.

This path deliberately skips executing real M-mode firmware. It must be labeled **direct boot**, not fully validated OpenSBI emulation.

### Target: firmware and disk boot

1. Reset the Hart in M-mode and execute boot ROM.
2. Run **real OpenSBI** with correct M-mode devices, PMP and trap behavior.
3. Hand off to WUWAOS's S-mode flash bootloader.
4. Expose the WUFS disk over VirtIO MMIO; bootloader loads `/boot/kernel.elf`.
5. Enter WUWAOS and verify shell, timer, page faults and block I/O.

WUWAOS's current `make run-disk` and `make run` paths provide reference behavior; they are not evidence WUWACP supports either path yet.

## 7. Virtual time and verification

- Isolate virtual time from host wall clock. Specify how instruction execution and device events advance Guest time before building timers; allow reproducible test mode.
- Trace architectural commits and trap state (PC, raw instruction, privilege, changed GPR/CSR, exception), with explicit opt-in.
- Use QEMU trace/architectural results as a differential reference; compare meaningful state at chosen checkpoints, not undefined log formatting.
- Failed Guest access produces a well-defined Guest fault/interrupt or modeled device error. Never dereference untrusted Guest Physical DMA addresses as unchecked host pointers.

## 8. Explicit deferrals

No JIT, cycle-accurate cache/pipeline, SMP, PCIe, complete Linux board, VirtIO GPU, keyboard or tablet in the first shell milestone. Add each only with a specific acceptance test and an approved roadmap change.
