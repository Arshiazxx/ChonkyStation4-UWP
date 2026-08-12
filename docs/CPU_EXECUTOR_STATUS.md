# M8 CPU Executor Status

Status: reviewed and intentionally retained as a synthetic test executor. No replacement CPU implementation is introduced by this pass.

## Scope

The code under `Core/CPU/` validates the process/thread/runtime foundations with a small private instruction format. It is not the PS4 CPU emulator, does not decode production PS4 binaries, and must not be treated as the native execution path.

The existing CPU README now makes this scope explicit. The class names `CpuExecutor` and `InstructionDecoder` are retained for the M8 API because they describe the local test harness; their supported byte format is explicitly synthetic and is not x86-64 machine code.

## Current implementation

### State model

`CpuState` contains:

- an x86-64-shaped general-purpose register file (`RAX` through `RDI`, `RBP`, `RSP`, and `R8` through `R15`);
- a 64-bit instruction pointer and stack pointer;
- arithmetic flags represented using architecturally useful x86-64 RFLAGS positions;
- execution state (`Ready`, `Running`, `Halted`, `Faulted`, or `StepLimitReached`);
- a small exception classification and diagnostic message.

This shape is deliberate: it keeps the M8 runtime and ABI tests aligned with the eventual x86-64 target. It does not mean the executor implements x86-64 instructions.

### Synthetic ISA

`Core/CPU/Instruction.hpp` defines a private `Opcode` enumeration. The decoder reads only this format:

| Synthetic instruction | Encoding size | Behavior |
|---|---:|---|
| `NOP` | 1 byte | Advances the instruction pointer. |
| `MOV immediate` | 10 bytes | Writes a 64-bit immediate to a selected synthetic register. |
| `ADD immediate` | 10 bytes | Adds a 64-bit immediate and updates the synthetic arithmetic flags. |
| `SUB immediate` | 10 bytes | Subtracts a 64-bit immediate and updates the synthetic arithmetic flags. |
| `LOAD` | 7 bytes | Reads an 8-byte value from a base register plus signed 32-bit displacement. |
| `STORE` | 7 bytes | Writes an 8-byte register value to a base register plus signed 32-bit displacement. |
| `HALT` | 1 byte | Stops the synthetic execution loop successfully. |

`InstructionDecoder` validates the private opcode, register indexes, encoded length, and truncation. Unknown bytes are reported as `invalid synthetic opcode`; they are not interpreted as real x86-64 instruction prefixes or opcodes.

`CpuExecutor` then:

1. fetches the synthetic byte record from `GuestMemory`;
2. decodes it with `InstructionDecoder`;
3. executes one of the supported synthetic operations;
4. checks guest-memory permissions and effective-address overflow;
5. records deterministic logs and fault state; and
6. stops on `HALT`, a fault, or the configured instruction limit.

## Test coverage

The existing M8 CPU smoke test covers:

- initial register, RIP, RSP, flags, and execution-state reset;
- immediate arithmetic (`10 + 5`, followed by subtraction);
- synthetic register-to-memory `STORE` and `LOAD`;
- guest-memory read/write validation;
- successful `HALT` and instruction count;
- arithmetic flag behavior;
- invalid synthetic opcode handling (`0xFE`); and
- fault-state reporting.

The M9 runtime smoke test also runs a synthetic program through the scheduler and exercises the exception boundary with an invalid synthetic opcode. M11/M12 module tests exercise the execution-context and backend boundary without executing guest code.

## Explicitly not implemented

The current executor does not provide:

- x86-64 variable-length instruction decoding;
- x86-64 prefixes, ModR/M, SIB, branches, calls, returns, SIMD, atomics, or system instructions;
- AMD Jaguar microarchitectural or compatibility emulation;
- real PS4 `eboot.bin` instruction execution;
- native PS4 ABI entry-point transfer;
- complete PS4 exceptions, interrupts, TLS, or privileged-state behavior; or
- a claim that real PS4 programs can run through M8.

The correct description is: **synthetic CPU state and instruction execution used by M8–M9 tests**.

## Execution backend abstraction

The execution layer is intentionally separate from `Core/CPU/`:

- `ExecutionContext` carries process/thread references, CPU-shaped state, instruction and stack pointers, ABI state, and exception state.
- `IExecutionBackend` defines the `Name`, availability, and `Start(ExecutionContext&)` boundary.
- The interface contract currently starts no guest or host code.
- `NativeExecutionBackend` reports that the native x86-64 boundary is available and accepts a valid context, but explicitly returns that guest execution is not enabled in M11.

This boundary can later host:

1. **Native x86-64 execution** after module mapping, relocations, TLS, stack setup, ABI bridging, exception routing, and security policy are complete.
2. **JIT/dynamic translation** for instrumentation, address translation, or compatibility cases that cannot use the native path directly.
3. **An upstream-compatible execution model** that starts linked modules, resolves HLE/sysmodule calls, and transfers a validated context to the appropriate runtime entry point.
4. **The existing synthetic M8 executor** as a deterministic test backend.

The abstraction is therefore extensible without making M8 responsible for every future execution strategy. The M8 executor should remain unchanged until a separate milestone authorizes native or translated execution work.

## Naming and documentation conclusion

No production class is named as a PS4 CPU emulator, and the existing source comments identify the private encoding as synthetic. The CPU README and this document explicitly state the limitation. The generic names `CpuExecutor` and `InstructionDecoder` are acceptable because their namespace, private `Opcode` type, diagnostics, and documentation define their test-only scope.

