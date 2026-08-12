# M10 ABI and Syscall Foundation

M10 adds the platform-neutral compatibility layer between CPU registers and the
M9 kernel syscall dispatcher.

## ABI design

The initial ABI models the PS4-compatible x86-64 register convention:

| Role | Register |
| --- | --- |
| Syscall number | `RAX` |
| Argument 0 | `RDI` |
| Argument 1 | `RSI` |
| Argument 2 | `RDX` |
| Argument 3 | `RCX` |
| Argument 4 | `R8` |
| Argument 5 | `R9` |
| Return value | `RAX` |

`ABI::AbiContext` provides typed access to the CPU state through this mapping.
`ABI::SyscallTransition` reads the syscall number and arguments, calls the
existing kernel dispatcher, writes the return value back to `RAX`, and emits
the ABI transition log.

## Syscall architecture

```text
CPU register state
        |
ABI::AbiContext
        |
ABI::SyscallTransition
        |
Kernel::SyscallDispatcher
        |
SyscallRegistry metadata + handler
        |
PS4-style kernel stub
        |
Return value in RAX
```

Each registered syscall has a number, name, category, argument count, status,
and handler. Unknown numbers return a safe ABI error value (`UINT64_MAX`) and
are logged without terminating execution.

## Initial syscall set

| Number | Name | Status |
| --- | --- | --- |
| `0x01` | `sceKernelExitProcess` | Implemented |
| `0x02` | `sceKernelGetProcessInfo` | Stub |
| `0x10` | `sceKernelCreateThread` | Stub |
| `0x11` | `sceKernelStartThread` | Stub |
| `0x12` | `sceKernelExitThread` | Stub |
| `0x20` | `sceKernelAllocateMemory` | Stub |
| `0x21` | `sceKernelReleaseMemory` | Stub |

The stubs validate their ABI arguments, use the existing process/thread/
GuestMemory abstractions, return safe values, and never terminate the host on
invalid input.

## Xbox ABI test

Launch the existing Xbox Dev Mode package and select `Run ABI Test`. The test
creates a process and initial thread, places syscall numbers and arguments in
the CPU registers, invokes `ABI::SyscallTransition`, and verifies register
returns for process, thread, memory, and unknown-syscall paths.

The successful output begins with:

```text
ChonkyStation4 ABI Test

Calling:
sceKernelCreateThread

Result:
SUCCESS

Return:
Thread ID 2
```

The report then includes the detailed ABI transition trace and ends with
`ABI smoke tests: PASS`.

## Future compatibility work

The next ABI steps are real guest-string and structure marshalling, syscall
number compatibility tables, TLS/thread-local state, error-number conventions,
and integration with a real x86-64 instruction path. The current M10 stubs are
intentionally deterministic and do not claim commercial-game compatibility.
