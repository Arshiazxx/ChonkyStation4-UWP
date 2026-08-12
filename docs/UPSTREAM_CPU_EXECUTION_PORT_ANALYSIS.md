# Upstream CPU Execution Port Analysis

Status: source audit and migration plan only. No upstream CPU/execution sources are integrated into the Xbox/UWP project by this document.

## Scope and conclusion

The upstream ChonkyStation4 project does not implement a separate Jaguar instruction interpreter. Its execution path loads PS4 x86-64 code, resolves the module environment, creates a host thread, and transfers control to the loaded module entry point using the host's x86-64 execution environment. Zydis is used to decode instructions for targeted code patching, and Xbyak is used to generate trampolines and patches; neither library is used as a general CPU emulator.

The upstream source is present in this workspace under `ChonkyStation4/`, so a second checkout was not created. The source-level porting path is now clear, but direct integration is not yet safe for Xbox/UWP. The main blockers are Windows/UWP executable-memory policy, the PS4 SysV ABI versus the Windows x64 ABI, MSVC x64 inline-assembly limitations, guest TLS construction, host-thread integration, and the difference between desktop and UWP platform services.

The current `Core/CPU/CpuExecutor` remains a synthetic M8 test harness. It must not be expanded into a PS4 CPU implementation and must not be used as the native execution path.

## Upstream execution call graph

The relevant upstream flow is:

```text
PS4::loadAndRun
  -> OS::Thread::init
  -> AppLoader / Linker::loadAndLink
  -> ELFLoader::load
  -> AppLoader::linkSysmodules / Linker::loadAndLinkLib
  -> HLE::buildHLEModule
  -> Linker::doRelocations
  -> App::run
  -> OS::Thread::createThread
  -> OS::Thread::threadStart
  -> initAndJumpToEntry
  -> native x86-64 transfer to Module::entry
```

The final step is an entry transfer, not an interpreter dispatch loop. Module initialization functions and the PS4 entry ABI are prepared before the transfer.

## Exact upstream source inventory

The following files are the CPU/execution-adjacent sources that must be considered for a real port.

| Upstream file | Responsibility | CPU/execution significance |
| --- | --- | --- |
| `ChonkyStation4/Common/Common.hpp` | Common types, helpers, and `PS4_FUNC` (`sysv_abi`) annotation | Defines the calling-convention assumption used by PS4-facing functions. |
| `ChonkyStation4/Loaders/ELF/ELFLoader.hpp` | ELF/SELF loader declarations and PS4 dynamic-segment constants | Describes the metadata needed before native entry execution: dynamic tags, dependencies, symbols, TLS, and special PS4 program headers. |
| `ChonkyStation4/Loaders/ELF/ELFLoader.cpp` | ELF/SELF parsing, host virtual-memory mapping, segment protection, dynamic metadata extraction, TLS and symbol table discovery | Places the guest x86-64 image in host address space and records the entry address used by the native transfer. |
| `ChonkyStation4/Loaders/ELF/CodePatcher.hpp` | Code-patching API | Declares targeted instruction patching, not general instruction execution. |
| `ChonkyStation4/Loaders/ELF/CodePatcher.cpp` | Zydis decoding and Xbyak-generated TLS/red-zone patches | Reads x86-64 instructions to patch PS4 TLS access and red-zone-sensitive stack references. It is not a CPU backend. |
| `ChonkyStation4/Loaders/Module.hpp` | Loaded-module metadata, segment records, TLS metadata, dynamic tables, exports, and generated stubs | Supplies the module and entry-point state consumed by linking and execution. It stores host pointers in the upstream desktop design. |
| `ChonkyStation4/Loaders/Symbol.hpp` | Symbol NID/name/library/module metadata and resolved pointer | Represents the symbol identity and host address used by relocations and calls. |
| `ChonkyStation4/Loaders/Linker/Linker.hpp` | Main/linker library loading and relocation API | Defines the boundary between module loading and executable entry. |
| `ChonkyStation4/Loaders/Linker/Linker.cpp` | Dependency loading, x86-64 relocation writes, unresolved-symbol reporting, and Xbyak trampolines | Makes imported calls reach HLE or unresolved-symbol handlers before native execution. |
| `ChonkyStation4/Loaders/App.hpp` | Application module collection and runtime lookup API | Owns the loaded module set, TLS image helpers, HLE lookup, and the run boundary. |
| `ChonkyStation4/Loaders/App.cpp` | Module initialization and native entry setup | Initializes modules and uses x86-64 register/stack setup followed by a jump to the executable entry. |
| `ChonkyStation4/Loaders/App/AppLoader.cpp` | Package/app discovery, `eboot.bin`, sysmodule and `.sprx` linking | Builds the runtime module environment before execution. |
| `ChonkyStation4/OS/Thread.hpp` | Host-thread abstraction, PS4 entry function type, guest TLS state | Defines how a loaded PS4 function is invoked on a host thread. |
| `ChonkyStation4/OS/Thread.cpp` | pthread host threads, guest TLS image/TCB setup, Windows TLS access, thread start | Establishes the host execution context and invokes the module/thread entry function. |
| `ChonkyStation4/OS/HLE.hpp` and `HLE.cpp` | HLE module and symbol stubs | Supplies host implementations for PS4-facing functions needed by linking and startup. |
| `ChonkyStation4/PlayStation4.cpp` | Runtime orchestration and load/run entry | Connects app loading, module linking, platform initialization, and `App::run`. |
| `ChonkyStation4/ChonkyStation4.cpp` | Desktop CLI bootstrap and large-address reservation | Desktop-only startup; it is not an Xbox/UWP entry-point implementation. |
| `ChonkyStation4/CMakeLists.txt` | Upstream source and dependency registration | Shows the required ELFIO, Xbyak, Zydis, pthread, SDL, graphics, and Windows-linker assumptions. |

## What the upstream code actually executes

### Native x86-64 execution

The upstream loader maps executable segments and retains an entry address. `App.cpp` prepares the PS4-style entry arguments and performs a native x86-64 control transfer. `OS::Thread.cpp` creates the host thread and invokes the selected entry function. There is no upstream `CpuExecutor` equivalent that decodes and interprets every PS4 instruction.

This is the important architectural fact for the Xbox port: the target and host are both x86-64, but that does not by itself make PS4 code executable on Xbox. The loader, ABI, imports, TLS, memory layout, exception behavior, executable-memory permissions, and platform services must all be compatible.

### Zydis and Xbyak

`CodePatcher.cpp` uses Zydis to inspect existing x86-64 instructions and Xbyak to generate small replacement sequences. `Linker.cpp` and `Module.hpp` also use Xbyak for unresolved-symbol and symbol-stub trampolines. These are narrow code-generation and patching facilities around a native execution path.

They should not be described as an x86-64 emulator, Jaguar emulator, or CPU interpreter.

### Execution context and host threads

Upstream does not expose a single platform-neutral `ExecutionContext` matching the current Xbox abstraction. Its effective context is distributed across:

- the module entry pointer and module metadata;
- the register/stack setup in `App.cpp`;
- the `PS4_FUNC` calling-convention annotation;
- host thread state in `OS::Thread`;
- guest TLS state stored in thread-local data;
- the loaded application/module collection in `App`.

The Xbox `Core/Execution/ExecutionContext` is therefore a useful adapter boundary, but it is not a direct upstream equivalent and should not be treated as proof that upstream native execution has already been ported.

## Comparison with the current Xbox workspace

| Upstream area | Current Xbox implementation | Assessment |
| --- | --- | --- |
| Native entry transfer | `Core/Execution/IExecutionBackend.hpp`, `UpstreamCompatibleX64Backend` | Correctly exposes a future boundary, but currently validates context and reports that guest execution is unavailable. It does not execute upstream code. |
| Synthetic execution | None in upstream; upstream runs native x86-64 code | `Core/CPU/CpuExecutor` and `SyntheticTestBackend` must remain as regression-test infrastructure only. Do not grow the synthetic ISA. |
| CPU register/context shape | `Core/CPU/CpuState`, registers, flags, and `Core/Execution/ExecutionContext` | Useful test and adapter data. It is not an upstream CPU implementation. Mapping to a native backend still requires ABI, stack, TLS, and exception decisions. |
| ELF loading | `Core/Loader/Elf64Loader` and `GuestMemory` | Safe, platform-neutral M7 foundation. It lacks the complete upstream SELF/package handling, PS4 dynamic tags, host-pointer mapping model, TLS setup, and executable-code policy. |
| Module loading | `Core/Loader/Module`, `ModuleManager`, `ModuleRegistry`, `LoadedModule` | M11/M12 metadata and process ownership foundation. It is not a drop-in replacement for upstream `Module`/`App`; an adapter is preferable to a second parallel module model. |
| Dependencies | `Core/Loader/Dependencies` | Safer and more explicit foundation for dependency names, ordering, missing modules, and cycles. It should become the compatibility-facing dependency layer. |
| Symbols | `Core/Loader/Symbols` | Platform-neutral metadata and lookup foundation. It does not yet produce upstream NID-aware host function pointers. |
| Relocations | `Core/Loader/Relocations` | Bounds-checked, failure-reporting foundation. It should remain the safety boundary; upstream's direct writes and panic-on-unsupported behavior must not be copied without equivalent validation. |
| ABI/syscalls | `Core/ABI`, syscall dispatcher, PS4 stubs | Existing Xbox compatibility layer. Upstream's `PS4_FUNC` and HLE/linker behavior must be adapted through this layer, not duplicated beside it. |
| HLE/sysmodules | `Core/Kernel` and current M10-M12 foundations | Current project does not contain the full upstream HLE/sysmodule set. This is a runtime-compatibility dependency of native execution, not a CPU feature. |
| Host threads/TLS | `Core/Kernel/Process/Thread`, scheduler, and execution context | M9 process/thread model is platform-neutral and testable. Upstream pthread/TLS implementation cannot be copied directly into UWP. |
| Code patching | No upstream-equivalent Zydis/Xbyak component in the Xbox project | Must be a separate, capability-gated Xbox component if needed. It is not a reason to modify `CpuExecutor`. |
| Platform bootstrap | UWP/Xbox host project | Must remain Xbox-specific. Upstream desktop CLI, filesystem paths, SDL/Vulkan setup, mounts, and desktop memory reservation are not direct Xbox sources. |

## Reuse, adaptation, and replacement decisions

### Candidates for reuse

The following upstream material is useful as a reference and may be reusable after review:

- module fields and lifecycle concepts from `Loaders/Module.hpp`;
- symbol identity fields from `Loaders/Symbol.hpp`;
- dynamic-tag and PS4 special-segment knowledge from `Loaders/ELF/ELFLoader.hpp`;
- linker sequencing and NID-based lookup concepts from `Loaders/Linker/Linker.cpp`;
- the `App` module collection and entry-initialization sequence from `Loaders/App.cpp`;
- the distinction between HLE exports, imported symbols, and unresolved-symbol handlers;
- the fact that the final execution boundary is a native x86-64 entry transfer.

These are source and behavior references, not an authorization to copy the upstream desktop implementation unchanged.

### Sources requiring Xbox/UWP adaptation

The following areas require an explicit compatibility implementation:

1. **Calling convention.** Upstream marks PS4-facing functions with `__attribute__((sysv_abi))`. The Xbox/UWP compiler and binary interface use the Windows x64 environment. A tested ABI bridge or a compiler-supported equivalent is required before calling native PS4 entry code.
2. **Executable memory.** Upstream uses Windows virtual-memory APIs and executable/writable patch buffers. Xbox/UWP restrictions, code-generation policy, page permissions, and MSIX deployment must be validated before adopting this model.
3. **Inline assembly.** The upstream x64 entry setup and TLS code use inline assembly patterns that cannot be copied directly into an MSVC x64 build. They need compiler intrinsics, a separately compiled assembly/helper boundary, or another tested mechanism.
4. **TLS.** Upstream reads Windows TLS internals and constructs a guest TLS image/TCB. UWP thread-local storage and Xbox process behavior need a dedicated implementation; the `gs`/`_tls_index` assumptions are not a portable Core API.
5. **Host threads.** Upstream uses pthread-compatible APIs and desktop thread behavior. The Xbox host should adapt the existing process/thread abstractions to the supported UWP threading primitives.
6. **Memory model.** Upstream stores direct host pointers and expects a compatible address-space layout. The current `GuestMemory`/module model is deliberately safer and platform-neutral. Any move to host-address execution must be explicit, bounded, and capability-tested.
7. **Package and platform services.** `AppLoader`, sysmodule paths, filesystem mounts, SDL/graphics initialization, and desktop startup must be mapped to Xbox/UWP services independently of the CPU boundary.
8. **Generated trampolines.** Xbyak/Zydis dependencies, generated-code lifetime, instruction-cache behavior, and executable-page transitions require Xbox/UWP build and runtime validation.

### What should be replaced eventually

No existing code should be deleted in this analysis milestone. Once the prerequisites are proven, the placeholder `UpstreamCompatibleX64Backend` is the component intended to be replaced or extended by an actual upstream-compatible native execution adapter.

The following must not be replaced:

- `SyntheticTestBackend` and the M8 synthetic tests;
- M7 ELF and guest-memory foundations;
- M9 process/thread/runtime tests;
- M10 ABI and syscall compatibility code;
- M11/M12 module, dependency, symbol, and safe-relocation foundations.

The following should not be copied into the Xbox project as-is:

- the upstream desktop `ChonkyStation4.cpp` bootstrap;
- desktop-only application paths and graphics initialization;
- inline x64 assembly without an Xbox-compatible boundary;
- unchecked relocation writes or unconditional process termination on unsupported input;
- a second independent module manager that bypasses `Core/Loader` and `Core/Kernel`.

## Direct integration decision

Directly integrating the upstream execution sources into the Xbox/UWP project is not yet safe. The source-level architecture is compatible in the broad sense—native x86-64 code can be the future execution path—but the concrete upstream implementation assumes desktop Windows, a SysV ABI annotation, pthread behavior, direct host pointers, inline assembly, private TLS details, and generated executable code.

Therefore this pass intentionally makes no core execution-code changes. The existing `UpstreamCompatibleX64Backend` remains an honest capability boundary, and the synthetic backend remains the only executable test backend. The CPU must not be reported as implemented until the actual upstream-compatible native execution code has been integrated and validated.

## Migration plan

### Phase 1 — Add upstream execution sources behind an adapter

1. Introduce a clearly named compatibility area for ported upstream execution sources rather than copying them into `Core/CPU`.
2. Port and test the metadata pieces first: module, symbol, dynamic-tag, dependency, and entry-state mapping.
3. Connect those pieces to the existing `Core/Loader`, `Core/ABI`, `Core/Kernel`, and `Core/Execution` interfaces through adapters.
4. Keep ELF/SELF parsing, relocation safety, and process ownership behind existing Xbox-facing interfaces.
5. Add ELFIO/Xbyak/Zydis or equivalent dependencies only after their Xbox/UWP build, licensing, generated-code, and executable-memory requirements are confirmed.
6. Exclude the upstream desktop bootstrap, graphics/runtime services, and unadapted pthread/inline-assembly code from the first port.

### Phase 2 — Replace the placeholder native backend

Replace or extend `UpstreamCompatibleX64Backend` only after all of the following have passing targeted tests:

- PS4-to-Windows-x64 ABI bridge;
- module entry register and stack setup;
- executable segment mapping and permission transitions;
- guest TLS creation and lookup;
- host-thread startup and teardown;
- exception/unwind boundary;
- imported symbol and HLE trampoline calls;
- safe relocation and unsupported-relocation reporting;
- Xbox/UWP deployment and execution policy.

The backend should perform a controlled native entry transfer, not implement a new instruction interpreter.

### Phase 3 — Keep the synthetic backend for tests

`SyntheticTestBackend` remains available for M8 regression tests, scheduler tests, and deterministic execution-context validation. It should not be expanded to approximate PS4 instructions and should not be selected for native module execution.

### Phase 4 — Validate on Xbox/UWP

Validation must include a Release x64 Xbox build, MSIX generation, module and dependency smoke tests, ABI bridge tests, TLS/thread tests, generated-code capability tests if required, and a controlled non-game native-entry fixture. Passing these tests would validate the porting boundary only; it would not establish commercial-game compatibility.

## Preserved current milestones

This analysis preserves the existing M7-M12 work:

- M7 ELF64 loading and guest memory mapping;
- M8 synthetic CPU state/execution and exception tests;
- M9 process, thread, scheduler, and runtime foundations;
- M10 ABI, syscall dispatcher, PS4 syscall stubs, and smoke tests;
- M11 module metadata, relocation framework, and execution boundary;
- M12 module manager, dependency, symbol, and safe-relocation foundations.

## Recommended next milestone

The next milestone should be **M13: upstream execution source port and Xbox capability gate**. It should begin with an adapter-level port of module-entry state, ABI bridging, TLS, executable-memory capability checks, and host-thread startup. It should not be called “PS4 CPU implementation” until the real upstream-compatible native execution path is integrated and verified.

## References

- Upstream repository: <https://github.com/liuk7071/ChonkyStation4>
- Upstream ELF loader: <https://github.com/liuk7071/ChonkyStation4/blob/master/ChonkyStation4/Loaders/ELF/ELFLoader.cpp>
- Upstream code patcher: <https://github.com/liuk7071/ChonkyStation4/blob/master/ChonkyStation4/Loaders/ELF/CodePatcher.cpp>
- Upstream linker and trampolines: <https://github.com/liuk7071/ChonkyStation4/blob/master/ChonkyStation4/Loaders/Linker/Linker.cpp>
- Upstream application entry setup: <https://github.com/liuk7071/ChonkyStation4/blob/master/ChonkyStation4/Loaders/App.cpp>
- Upstream host-thread and TLS setup: <https://github.com/liuk7071/ChonkyStation4/blob/master/ChonkyStation4/OS/Thread.cpp>
- Upstream build/dependency definition: <https://github.com/liuk7071/ChonkyStation4/blob/master/CMakeLists.txt>
