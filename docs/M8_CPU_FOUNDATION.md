# M8 CPU Foundation

M8 adds a platform-neutral execution harness to the existing M7 loader and
guest-memory foundation.

```text
CpuState
    -> CpuExecutor::Fetch
        -> GuestMemory::Read(RIP)
    -> InstructionDecoder
    -> synthetic execution dispatcher
        -> Registers / Flags
        -> GuestMemory::Read / Write
    -> HALT or structured fault report
```

## State model

The register file is shaped for future x86-64 compatibility: RAX–R15,
instruction pointer (RIP), stack pointer (RSP), and architecturally useful
RFLAGS bits (carry, zero, sign, and overflow). `CpuState` also records the
execution state, instruction count, and exception text.

## M8 validation encoding

The current byte encoding is intentionally synthetic and is not a real PS4
instruction decoder:

- `NOP` — one byte
- `MOV reg, immediate64`
- `ADD reg, immediate64`
- `SUB reg, immediate64`
- `LOAD destination, [base + displacement32]`
- `STORE source, [base + displacement32]`
- `HALT` — one byte

`RunCpuSmokeTests` maps a synthetic program and data region in `GuestMemory`,
executes arithmetic and memory operations, validates HALT, and separately
checks invalid-instruction faulting. The Xbox `Run CPU Test` button displays
the same structured report through the existing HostServices log.

Real PS4 eboot execution, complete x86-64 decode, exceptions/interrupts, TLS,
relocations, and executable host memory remain future milestones.
