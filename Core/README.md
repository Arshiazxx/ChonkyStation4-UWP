# ChonkyStation4 Core Foundation

This directory contains platform-neutral emulator primitives.  Core code must
not include UWP, Xbox, Win32, SDL, or graphics API headers.  A host owns
storage, logging, lifecycle, input, and presentation and calls Core through
small C++ interfaces.

Current foundation:

- `Common/` — guest address types shared by core subsystems.
- `CPU/` — x86-64-shaped state and a synthetic instruction execution harness.
- `Loader/` — ELF64 inspection and loadable-segment reporting.
- `Memory/` — guest virtual-address region tracking and guarded reads/writes.
- `Kernel/` — process, thread, scheduler, syscall, exception, and runtime
  smoke-test foundations for M9.
- `ABI/` — PS4-style x86-64 argument mapping, return values, syscall
  transitions, and ABI smoke tests for M10.
- `FileSystem/` and `GPU/` — reserved subsystem boundaries for later milestones.

The Xbox host currently uses `Loader/Elf64Loader` for the developer test path.
It also exposes `CPU/RunCpuSmokeTests` through the `Run CPU Test` developer
button. M9 adds `Kernel/RunRuntimeSmokeTests` through the `Run Runtime Test`
developer button. Both paths operate on Core interfaces and do not execute
native guest code.
M10 adds `ABI/RunAbiSmokeTests` through the `Run ABI Test` developer button.
The existing PS4-specific loader under `ChonkyStation4/Loaders/ELF` remains
intact and is not replaced by this foundation.
