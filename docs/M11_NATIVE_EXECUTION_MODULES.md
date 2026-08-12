# M11 Native Execution and Module Loading Foundation

M11 extends the M7 ELF loader and the M9 process model with the metadata and
boundaries needed for future native PS4 user-mode execution. It does not claim
that commercial PS4 games can execute on Xbox yet.

## M7 loader audit

The existing platform-neutral `Elf64Loader` now provides the following
foundation:

- It validates the ELF magic, 64-bit class, little-endian encoding, ELF
  identification version, header size, ELF version, and x86-64 machine type.
- It decodes the ELF header and all program headers, with bounds checks for the
  program-header table and each file-backed segment.
- It selects `PT_LOAD` entries, rejects `p_memsz < p_filesz`, rejects file and
  virtual-address overflow, and validates non-trivial power-of-two
  `p_align` values and their file/virtual congruence.
- It reports the ELF entry point and maps each loadable segment at its guest
  virtual address. `PT_LOAD` flags become guest read/write/execute
  permissions.
- `GuestMemory::Map` initializes the entire mapped allocation to zero, then
  copies the file-backed prefix. This supplies the required BSS/zero-fill
  behavior for `p_memsz - p_filesz`.
- Failed multi-segment loads unmap the segments that were already mapped.

The loader still intentionally has limitations. It does not yet parse section
tables, dynamic tables, symbol tables, `PT_INTERP`, thread-local storage, or
PS4-specific loader metadata. It maps the current ELF virtual addresses
directly; a complete `ET_DYN` load-bias policy and page-granular permission
transition are future work. Overlapping or page-sharing segments also require
the future image mapper because the current `GuestMemory` model stores
non-overlapping regions.

`LoadBytes` and `LoadBytesIntoMemory` provide the same validation and mapping
path for deterministic platform-neutral tests without requiring a host file
system.

## Module loading architecture

`Core/Loader/Module/LoadedModule` is the process-facing module record. It
tracks:

- module name and source path;
- module kind (main executable or shared object);
- mapped image base and image size;
- entry point and loaded state;
- PT_LOAD segment metadata;
- future-ready symbol, dependency, and relocation collections.

`Process` owns a collection of `LoadedModule` instances and separately tracks
the main executable. Existing `LoadedExecutable` data remains available for
M7/M9 callers, while new code can use the module collection as the common
image model.

The M11 module smoke test builds a small ELF64 fixture named `eboot.bin`, loads
it through the real loader, registers it as `Main Executable`, creates PID 1
and TID 1, resolves the entry point, and reaches the execution backend
boundary. It never executes the fixture bytes.

## Relocation architecture

`Core/Loader/Relocations` defines a relocation type abstraction, relocation
records, a resolver interface, structured status, and diagnostic text.
`SafeRelocationResolver` currently implements only the bounded
`R_X86_64_RELATIVE` shape. It checks the target offset and address arithmetic,
and delegates the final write to `GuestMemory`, so guest permissions and
mapping checks remain authoritative.

Unsupported symbolic relocations such as absolute, GOT, and PLT forms return
`Unsupported`; out-of-range targets return `OutOfBounds`. Neither case writes
memory. Dynamic relocation-table parsing, symbol lookup, dependency loading,
and PS4 relocation coverage are deliberately deferred.

## Native execution architecture

`ExecutionContext` snapshots the guest CPU state needed to start a thread:
instruction pointer, stack pointer, general registers, ABI view, process and
thread references, and exception state. `IExecutionBackend` defines the
backend-neutral boundary. `NativeExecutionBackend` currently reports that the
boundary is available and accepts a valid context, but marks execution as not
performed.

This leaves room for three independent future implementations:

1. a native x86-64 execution backend with explicit host/guest safety rules;
2. a dynamic translation or JIT backend;
3. the existing M8 synthetic executor for deterministic tests and diagnostics.

The synthetic M8 executor remains available and is not replaced by a new
interpreter. The M11 interface only defines how a scheduler-produced context
will be handed to one of these implementations.

## Why no separate Jaguar interpreter is the primary path

The PS4 user-mode ABI and executable environment target x86-64. Xbox Series
hardware is also x86-64, so the architectural relationship is a useful basis
for a future native execution path. That does not make PS4 binaries directly
compatible: system calls, memory layout, loader behavior, graphics/audio
services, security boundaries, libraries, and platform contracts still need a
compatibility layer. It does mean that a separate Jaguar instruction-set
interpreter is not the primary direction for PS4 user-mode code. The existing
synthetic CPU remains a test abstraction, while M11 establishes the native
execution boundary and PS4 software/runtime environment around it.

## Current limitations and next step

M11 does not execute arbitrary guest instructions, perform host-code
trampolines, load real PS4 dynamic dependencies, parse symbol/relocation
sections, or provide complete PS4 kernel and library compatibility. It also
does not claim that real PS4 games boot or run.

The recommended M12 direction is a controlled dynamic ELF/module loader:
parse `PT_DYNAMIC`, symbol and relocation tables; add an explicit load-bias
and page-mapping policy; resolve dependencies through a process module
manager; and define the security boundary for the first native x86-64 backend.
