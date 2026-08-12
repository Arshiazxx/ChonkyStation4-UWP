# CPU Architecture Verification

Status: verification pass completed. This document records the architecture relationship and the purpose of the existing M8/M11 execution code. No core execution code required modification.

## 1. Target architecture

### PlayStation 4

Sony's PS4 specifications identify the main CPU as an eight-core AMD Jaguar processor with an x86-64 ISA. See the [official PS4 specifications](https://www.playstation.com/en-us/ps4/tech-specs/) and the [PS4 Pro safety guide specifications](https://www.playstation.com/content/dam/global_pdc/en/corporate/support/manuals/ps4-docs/cuh-7116b-cuh-7108b-cuh-7102b/PS4_Pro_Chassis_CUH-7108_Safety_Guide_RUS_IND_EN_Web.pdf).

For this project, the important distinction is:

- Jaguar is the PS4 CPU microarchitecture/family.
- x86-64/AMD64 is the instruction-set target relevant to user-mode code.
- The runtime ABI, register names, calling convention, ELF machine type, and native execution boundary should therefore be x86-64-oriented.

This is an ISA compatibility target, not a requirement to reproduce Jaguar's cycle timing, cache behavior, or exact microarchitectural performance.

### Xbox Series X|S

Microsoft describes Xbox Series X as using a custom eight-core Zen 2 CPU. Microsoft's GDK names the Series target `Gaming.Xbox.Scarlett.x64` and exposes `_M_X64` as the processor-architecture define for x64 code. See the [Xbox Series hardware overview](https://news.xbox.com/en-us/2020/06/10/everything-you-need-to-know-about-the-future-of-xbox-so-far/) and [Microsoft GDK preprocessor definitions](https://learn.microsoft.com/en-us/xbox/gdk/docs/tools/tools-console/visualstudio/preprocessor-definitions?view=gdk-2604).

The current workspace's Xbox/UWP project is configured for `Debug|x64` and `Release|x64`. That is consistent with an AMD64/x86-64 host execution environment. It is important not to conflate the current UWP packaging model with a native Scarlett GDK title: the project is an Xbox/UWP host targeting x64, while a future native console configuration would use the appropriate GDK platform target.

At the ISA level, PS4 Jaguar and Xbox Series Zen 2 are different CPU microarchitectures implementing the same relevant x86-64 family. That makes native x86-64 execution a technically coherent future direction, while leaving PS4-specific ABI, OS, memory, GPU, and module behavior as separate compatibility work.

## 2. Current M8 CPU foundation

The M8 CPU code is a synthetic execution and testing model, not a PS4 instruction decoder and not a second-ISA emulator.

Evidence in the current source:

- `Core/CPU/README.md` describes an x86-64-shaped register file and explicitly says the executor does not decode native PS4 x86-64 instructions or execute real `eboot.bin` code.
- `Core/CPU/Instruction.hpp` documents a deliberately private M8 encoding rather than claiming that the byte values are x86-64 opcodes.
- `Core/CPU/Instruction.cpp` decodes only the small M8 operation set: NOP, immediate moves/add/subtract, load, store, and halt.
- `Core/CPU/CpuExecutor.cpp` fetches those synthetic records from `GuestMemory`, executes them for deterministic tests, and reports memory, address, decode, and step-limit failures.
- `Core/CPU/CpuSmokeTests.cpp` constructs synthetic byte programs and validates arithmetic, memory access, halt behavior, and invalid-instruction handling.

The register model is intentionally x86-64-shaped: it contains RAX through RDI, RBP/RSP, and R8 through R15, with a 64-bit instruction pointer and stack pointer. This is appropriate as a state abstraction for the intended target, but it is not evidence that M8 can execute real x86-64 machine code.

### ISA mismatch check

No ARM, ARM64, PowerPC, or other non-x86 instruction decoder or register model was found in the reviewed `Core/CPU` and `Core/Execution` implementation. The project and solution target x64; any ARM/ARM64 branches found in generated packaging scripts are deployment-tooling branches, not guest CPU execution paths.

Conclusion: M8 is compatible with the architecture plan. It is a deliberately synthetic harness with x86-64-shaped state, not an attempt to emulate a different ISA.

## 3. Current execution backend design

`Core/Execution/IExecutionBackend.hpp` defines a backend-neutral boundary around an `ExecutionContext`. The context carries:

- instruction pointer and stack pointer;
- general-purpose register state and CPU exception state;
- ABI state;
- process and thread references.

The interface exposes `Name()`, `IsAvailable()`, and `Start(ExecutionContext&)`. Its contract currently prevents guest or host code from being executed at the boundary. The implementation in `NativeExecutionBackend.cpp` reports that the native x86-64 boundary is available and accepted, but intentionally does not execute guest code yet.

The interface can therefore support multiple future implementations without changing the process/thread abstractions:

1. **Native x86-64 backend** — maps validated guest code into a controlled host-address space, establishes the PS4-compatible stack/TLS/ABI state, and transfers control under an explicit exception and security policy.
2. **JIT/dynamic-translation backend** — translates selected guest blocks when native execution needs instrumentation, address translation, or compatibility handling.
3. **Synthetic M8 backend** — remains available for unit/smoke tests and deterministic scheduler/runtime validation.

The backend name `Native x86-64 execution boundary` is aligned with the verified target. It does not overclaim current native execution; the current implementation is only a safe handoff boundary.

## 4. Future execution strategy

The recommended strategy is:

1. Keep M8 synthetic execution unchanged as a test backend.
2. Complete upstream-compatible ELF/module/linker work before native entry-point transfer: dynamic metadata, relocations, NID symbols, TLS, module initialization, and syscall/HLE bindings.
3. Define the native backend's guest-address/host-address mapping, stack layout, register preservation, exception routing, and thread lifetime.
4. Add native x86-64 execution only behind the existing `IExecutionBackend` interface and only for validated, explicitly authorized code.
5. Add JIT or block translation only where it solves a concrete compatibility or instrumentation requirement; it is not required merely because PS4 and Xbox use different microarchitectures.
6. Keep PS4-specific kernel, filesystem, module, and GPU compatibility separate from CPU ISA handling.

This strategy uses the Xbox CPU as a possible x86-64 execution host, but does not imply that real PS4 games can execute today. Native CPU compatibility alone is insufficient without the PS4 runtime environment.

## 5. Verification result

| Question | Result |
|---|---|
| PS4 CPU target | AMD Jaguar, x86-64 |
| Xbox Series CPU environment | Custom AMD Zen 2, x64/x86-64 execution target |
| M8 purpose | Synthetic deterministic execution/testing |
| M8 alternate-ISA assumptions | None found; no ARM/PowerPC execution path |
| Native/JIT extensibility | Supported by `IExecutionBackend` and `ExecutionContext` |
| Core execution changes required | None |

