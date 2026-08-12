# M12 Dynamic ELF Module Loading

M12 extends the M11 single-module foundation into a process-owned dynamic
module environment. It is still a runtime foundation: it does not claim that
commercial PS4 software can boot or run on Xbox.

## PS4 module model

A PS4 process is expected to contain a main executable such as `eboot.bin`,
shared objects such as `libkernel.sprx`, and additional system/runtime
modules. M12 represents each image as a `LoadedModule` and keeps those modules
in a `ModuleRegistry` owned by `ModuleManager`. The manager uses the existing
ELF loader and guest memory object, so file-backed and deterministic byte-backed
loads follow the same validation and zero-fill path.

Each module records its name, source path, kind, load state, base, mapped size,
entry point, segments, dependencies, exported symbols, imported symbols, and
relocations. `ModuleManager` provides load, unload, lookup, main-module, and
process-facing resolution operations. M11’s compatibility `Process::Modules`
view remains available while `Process::ModuleManager` is now the authoritative
module owner.

## Dependency resolution

`DependencyResolver` walks registered module dependencies depth-first and emits
a dependency-first load order. It reports required missing modules and detects
cycles using visiting/visited states. Optional missing dependencies do not
fail resolution. A successful resolution marks the root module’s dependencies
as resolved; it does not invent or hardcode PS4 library addresses.

The current resolver operates on already registered modules. Future work will
connect dependency names to a controlled search path and module loader policy.

## Symbols

`SymbolInfo` now distinguishes exported and imported symbols and tracks binding,
type, relative/absolute value semantics, and resolved import state.
`SymbolResolver` searches registered module exports and computes a relative
symbol address from the module base. It reports the provider module and
resolved address, while missing required imports remain explicit failures.
Weak imports are allowed to remain unresolved. No real PS4 address is embedded
in the implementation.

## Relocation handling

The M11 relocation interface is expanded in M12:

- `R_X86_64_RELATIVE` writes `moduleBase + addend`.
- `R_X86_64_64` writes `resolvedSymbol + addend`.
- `R_X86_64_GLOB_DAT` and `R_X86_64_JUMP_SLOT` use the same bounded symbol
  write shape as a safe foundation.
- `R_X86_64_NONE` is a no-op.

Every relocation checks the module-relative target range, integer overflow,
symbol availability, and guest-memory write permissions. Unsupported types,
missing symbols, unmapped targets, and invalid arithmetic return structured
failure statuses and do not write memory. `ModuleManager::ApplyRelocations`
collects per-record results and logs.

This is not a complete PS4 relocation implementation. Dynamic table parsing,
symbol-version rules, TLS relocations, copy relocations, PLT behavior, and PS4
loader-specific policy remain future work.

## Runtime and Xbox integration

The M12 smoke test creates PID 1, loads byte-backed `eboot.bin` and
`libkernel.sprx` fixtures, registers a dependency, adds an export/import pair,
resolves the dependency and symbols, processes safe relocations, checks missing
and circular dependency reporting, rejects an unsupported relocation, and
passes the resulting thread context to the existing non-executing backend
boundary.

The Xbox UI exposes this through `Run Dependency Test`, alongside the M7-M11
buttons. The test reaches real Core module, dependency, symbol, relocation,
process, and execution abstractions; it does not execute arbitrary host code.

## Limitations and future libkernel compatibility

M12 does not parse a complete PS4 `PT_DYNAMIC` environment, discover real SPRX
files, implement library ABI compatibility, provide syscall behavior beyond
the existing M10 stubs, or execute real user-mode instructions. The current
native backend remains an explicit boundary only. The synthetic M8 CPU test
system remains available for deterministic execution tests.

The recommended M13 direction is a controlled PS4 runtime library layer:
dynamic-section and symbol-table ingestion, a module search/policy system,
versioned libkernel compatibility surfaces, and a security-reviewed native
x86-64 execution handoff. These steps are prerequisites for meaningful native
user-mode experiments and are not a claim of commercial compatibility.
