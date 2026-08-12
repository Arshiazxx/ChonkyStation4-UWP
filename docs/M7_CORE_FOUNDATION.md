# Emulator Core Foundation — ELF64 and Guest Memory

This checkpoint adds a platform-neutral `Core/` layer beside the existing
PS4-specific runtime. The Xbox host calls this layer through ordinary C++
interfaces; Core does not include UWP, Xbox, Win32, SDL, D3D12, or Vulkan
headers.

## Current path

```text
Xbox MainPage
    -> Core::Loader::Elf64Loader
        -> ELF64 header/program-header validation
        -> PT_LOAD segment report
    -> existing Xbox HostServices logging
```

The developer button `Load Test ELF` inspects
`ApplicationData::Current->LocalFolder\eboot.bin` and writes the structured
loader report to the existing debug log and status text. The file is not
executed.

## Core memory contract

`Core/Memory/GuestMemory` tracks non-overlapping guest virtual-address regions,
stores zero-initialized backing bytes, enforces read/write permissions, and
supports reads and writes across adjacent mappings. `Elf64Loader::LoadIntoMemory`
maps validated `PT_LOAD` segments into this abstraction and zero-fills the
memory-size tail. It does not reserve the PS4 address space or create
executable host memory.

The existing `ChonkyStation4/Loaders/ELF` implementation and
`platform/xbox/Memory/XboxGuestMemory` diagnostic remain intact. They serve
different boundaries and are intentionally not replaced by this foundation.

## Next milestone

Add a platform-neutral CPU state/exception contract and a small interpreter
test path that can execute synthetic guest instructions from `GuestMemory`
without entering native PS4 code. Native executable mappings, relocations,
TLS/ABI behavior, and full PS4 service dispatch remain later work.
