# M9 Runtime Foundation

M9 establishes the platform-neutral process environment used by the Xbox host
and future PS4 service implementations.

## Execution flow

```text
ELF64 loader
    |
GuestMemory (process address space)
    |
Process
    |
Thread (CpuState + stack metadata)
    |
Scheduler
    |
CpuExecutor
    |
ExceptionBoundary / SyscallDispatcher
```

`Process::LoadExecutable` preserves the loader report and maps PT_LOAD segments
into the process address space. `Scheduler` creates threads with an initialized
RIP/RSP and executes each ready thread sequentially. CPU failures are converted
into runtime exceptions and reported through the exception boundary.

## M9 services

- `Process`: PID, address-space reference, executable metadata, state, exit and fault status.
- `Thread`: TID, CPU state, stack metadata, state, exit and fault status.
- `Scheduler`: thread creation and a basic non-preemptive execution loop.
- `SyscallDispatcher`: handler registration and syscall `0x01` (`ProcessExit`).
- `ExceptionBoundary`: CPU, memory, invalid-instruction, and syscall-transition handling.

## Current limitations

There is no preemption, blocking/wakeup, thread priority, real syscall instruction,
PS4 ABI, kernel object table, signal delivery, or host-backed process isolation.
The M9 runtime test uses the M8 synthetic instruction set to validate the control
flow and fault boundary.

## Xbox runtime test

Build and deploy the existing Release Xbox package, launch ChonkyStation4, and
select `Run Runtime Test` in the diagnostic button row. The test creates a
process and thread, executes a synthetic program through the scheduler, dispatches
the registered `ProcessExit` syscall (`0x01`), and writes the complete result to
the on-screen status field and the existing host log.

Expected successful output includes:

```text
ChonkyStation4 Runtime Test

Process created:
PID 1

Thread created:
TID 1

Execution started

ChonkyStation4 Syscall

Number:
0x01

Handler:
ProcessExit

Result:
Success
```

`Load Test ELF` and `Run CPU Test` remain available and are independent of this
runtime diagnostic path.
