# CPU Execution Alignment

Status: backend architecture alignment with upstream ChonkyStation4. This milestone adds explicit backend adapters and validation only. It does not add a Jaguar interpreter, expand the M8 synthetic ISA, or execute arbitrary guest code.

## PS4 CPU architecture

The PS4 user-mode target is AMD Jaguar x86-64. Jaguar is the CPU microarchitecture; x86-64/AMD64 is the instruction-set and ABI-relevant target. The execution design therefore needs PS4-compatible x86-64 register, stack, calling-convention, module, TLS, and exception handling. It does not need cycle-accurate Jaguar emulation merely because the Xbox host uses a different x86-64 microarchitecture.

## Upstream ChonkyStation4 execution method

The upstream reference is [liuk7071/ChonkyStation4](https://github.com/liuk7071/ChonkyStation4). The relevant execution path is distributed across its loader, linker, application, OS-thread, and code-generation layers:

1. `Loaders/ELF/ELFLoader.*` maps ELF/SELF segments into host-backed memory, records a host module base and host entry pointer, extracts dynamic metadata, and prepares TLS and relocation information.
2. `Loaders/Linker/Linker.*` links the main module and libraries, applies x86-64 relocations, resolves qualified symbols, and creates generated x86-64 trampolines for unresolved calls using Xbyak.
3. `Loaders/App.*` owns the ordered module list, module lookup, TLS image lookup, and application start sequence.
4. `OS/Thread.*` creates host threads and keeps guest TLS state associated with them.
5. The application start path initializes modules and ultimately transfers control to the linked module entry point using the host x86-64 execution environment.

This is a native-host execution strategy with runtime-generated bridging code, not a general-purpose alternate-ISA interpreter. It is still a compatibility runtime: PS4 ABI behavior, HLE/sysmodules, filesystem, memory layout, TLS, exceptions, and GPU services must match well enough for guest code to function.

## Current Xbox execution method

The Xbox/UWP workspace currently has two distinct layers:

### M8 synthetic execution

`Core/CPU/CpuExecutor` and `InstructionDecoder` implement a private test format with only NOP, immediate move/add/subtract, load, store, and halt. The M8 register state is x86-64-shaped so later ABI/runtime tests use the correct general-purpose register vocabulary, but the byte format is not x86-64 machine code.

The M8 executor remains required for M8/M9 regression tests. It must not grow into a fake Jaguar interpreter and must not be used to claim native PS4 execution.

### Backend boundary

`Core/Execution/ExecutionContext` carries process/thread references, CPU-shaped state, instruction and stack pointers, ABI state, and exception state. `IExecutionBackend` (also available as the architectural alias `ExecutionBackend`) is the boundary for selecting an execution strategy.

The current `UpstreamCompatibleX64Backend` reports an available x86-64 boundary and accepts a valid context, but does not transfer control to guest code. This is intentional: module mapping, dynamic linking, TLS, ABI bridging, exception routing, and a safe host/guest address policy are prerequisites.

The existing `NativeExecutionBackend` name remains as a compatibility wrapper for M11/M12 callers. New code should use `UpstreamCompatibleX64Backend` so its boundary-only status is explicit.

## Backend architecture

```text
ExecutionBackend / IExecutionBackend
├── SyntheticTestBackend
│   └── adapts the existing M8 CpuExecutor; private test ISA only
└── UpstreamCompatibleX64Backend
    └── upstream-compatible x86-64 boundary; no guest transfer yet
```

### `SyntheticTestBackend`

This adapter selects the existing `CpuExecutor` through the common backend interface. It is deliberately narrow:

- it executes only the current M8 synthetic encoding;
- it uses the existing `GuestMemory` and CPU state;
- it reports success only when the synthetic program halts;
- it does not decode or execute real PS4/x86-64 bytes.

### `UpstreamCompatibleX64Backend`

This adapter establishes the future native path's contract:

- it is available only when the host compilation target is x86-64;
- it validates the `ExecutionContext`;
- it does not execute guest code in the current milestone;
- it is the location for future upstream-compatible module entry transfer, not a Jaguar emulator.

## Reuse, replacement, and adapter decisions

| Current component | Decision | Reason |
|---|---|---|
| `CpuState`, `Registers`, `Flags` | Reuse as execution-context state | The x86-64-shaped state is useful for ABI, scheduler, and backend contracts. |
| `CpuExecutor` / `InstructionDecoder` | Keep unchanged as M8 test implementation | It provides deterministic regression coverage and is not a native decoder. |
| `ExecutionContext` | Reuse and extend only as needed | It already carries process, thread, register, RIP/RSP, ABI, and exception state. |
| `IExecutionBackend` / `ExecutionBackend` | Reuse as common adapter interface | It separates execution strategy from process/module/runtime ownership. |
| `SyntheticTestBackend` | Add as M8 adapter | Makes the retained synthetic path explicit and selectable. |
| `UpstreamCompatibleX64Backend` | Add as upstream-aligned boundary | Provides the correct future native x86-64 integration point without pretending to execute yet. |
| `NativeExecutionBackend` | Preserve as compatibility wrapper | Existing M11/M12 callers continue to build while new code uses the explicit upstream-compatible name. |
| M9 `Scheduler` | Keep current synthetic path for regression tests | Scheduler behavior is already validated; moving it to native execution requires a separate runtime contract. |
| Upstream ELF/linker/module layers | Integrate through adapters, not duplicate CPU logic | Native execution depends on host module pointers, relocations, TLS, NID symbols, HLE, and thread setup. |

## CPU backend validation

The new platform-neutral smoke test exercises both adapters with a valid process/thread context and a small existing-format M8 program:

```text
CPU Backend Test

Synthetic backend:
PASS

Upstream compatible backend:
AVAILABLE

Architecture:
x86-64

Result:
SUCCESS
```

`AVAILABLE` means the x86-64 boundary is present and safely accepts a valid context. It does not mean that native guest code was executed.

## Migration plan

### Phase 1 — Keep the split stable

Use `SyntheticTestBackend` for deterministic M8/M9 tests and `UpstreamCompatibleX64Backend` for module/execution-boundary tests. Do not add synthetic opcodes or make the M8 executor parse real ELF instruction bytes.

### Phase 2 — Complete the upstream-compatible startup contract

Connect the backend to the existing M11/M12 module abstractions after they can represent upstream-relevant dynamic tables, segment permissions, TLS, NID-qualified symbols, dependencies, relocations, and module initialization. Add explicit host-address/guest-address translation and process-owned module lifetime.

### Phase 3 — Bridge ABI and services

Route resolved upstream-style library/NID calls through the current x86-64 ABI and syscall/HLE boundaries. Define thread start, stack alignment, TLS, exception, and exit behavior before enabling entry transfer.

### Phase 4 — Implement controlled native x86-64 transfer

Only after the previous contracts are tested should `UpstreamCompatibleX64Backend` gain an opt-in native execution implementation. It should transfer control to validated host-mapped x86-64 code under an explicit security and failure policy. It should not emulate Jaguar microarchitecture.

### Phase 5 — Add JIT/dynamic translation only where justified

A JIT can be introduced as another `ExecutionBackend` implementation for instrumentation, address translation, unsupported runtime cases, or debugging. It is optional and must not replace the M8 regression backend.

## Current limitations

- The upstream-compatible backend is a boundary-only implementation.
- No real PS4 x86-64 instructions are executed by the Xbox Core path.
- The M8 synthetic ISA is intentionally incomplete and will not be expanded as part of alignment.
- Native PS4 execution still requires upstream-compatible loader/linker, ABI, TLS, HLE/sysmodules, filesystem, exception, and GPU work.
- No ARM, ARM64, or PowerPC execution path is introduced.
- Real PS4 game compatibility is not claimed.

