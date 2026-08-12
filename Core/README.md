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
- `CPU/`, `Kernel/`, `FileSystem/`, and `GPU/` — reserved subsystem boundaries;
  implementation will be added as each subsystem gets a real contract.

The Xbox host currently uses `Loader/Elf64Loader` for the developer test path.
It also exposes `CPU/RunCpuSmokeTests` through the `Run CPU Test` developer
button. Both paths operate on Core interfaces and do not execute native guest
code.
The existing PS4-specific loader under `ChonkyStation4/Loaders/ELF` remains
intact and is not replaced by this foundation.
