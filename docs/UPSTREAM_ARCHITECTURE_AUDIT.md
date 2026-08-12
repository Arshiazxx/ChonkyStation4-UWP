# Upstream ChonkyStation4 Architecture Audit

Status: architecture audit and integration plan only. No emulator subsystem was added or removed for this milestone.

## Scope and reference source

The upstream reference is [liuk7071/ChonkyStation4](https://github.com/liuk7071/ChonkyStation4), primarily its [`ChonkyStation4/`](https://github.com/liuk7071/ChonkyStation4/tree/master/ChonkyStation4) source tree and [`CMakeLists.txt`](https://github.com/liuk7071/ChonkyStation4/blob/master/CMakeLists.txt). This workspace already contains an upstream reference tree at `ChonkyStation4/`; it was inspected in place. A second repository or checkout was not created.

The upstream project and the Xbox/UWP port have different responsibilities. Upstream is a desktop-oriented PS4 runtime with native host execution, HLE libraries, a linker, and a GCN/Vulkan path. The Xbox project is currently a UWP host and a platform-neutral milestone testbed. The correct integration direction is therefore to reuse the existing Xbox boundaries where they are useful, then connect them to upstream-compatible loader/runtime semantics in deliberate stages.

## Upstream component map

| Area | Upstream location | Observed responsibility |
|---|---|---|
| Bootstrap and runtime ownership | `ChonkyStation4/PlayStation4.cpp`, `PlayStation4.hpp`, `ChonkyStation4.cpp` | Initializes the OS and graphics services, prepares an application, links modules, mounts guest devices, and runs the linked application. |
| Application model | `Loaders/App.hpp`, `Loaders/App.cpp` | Owns the ordered module list, application metadata, unresolved-symbol handlers, TLS image access, and the transition to the application entry point. |
| Application/package loading | `Loaders/App/AppLoader.*`, `Loaders/SFO/SFOLoader.*` | Recognizes an application directory, locates `eboot.bin` and `sce_sys/param.sfo`, parses metadata, links system modules, and loads modules from `sce_module`. |
| ELF/SELF loading | `Loaders/ELF/ELFLoader.*` | Loads ELF through ELFIO and has explicit handling for SELF headers, `PT_LOAD`, `PT_SCE_RELRO`, `PT_DYNAMIC`, `PT_SCE_DYNLIBDATA`, `PT_SCE_PROCPARAM`, `PT_TLS`, and `PT_GNU_EH_FRAME`. It records dynamic tables, TLS, symbols, libraries, and segment metadata. |
| Code patching | `Loaders/ELF/CodePatcher.*` | Applies title- or runtime-specific patches after loading. This is separate from ordinary ELF relocation. |
| Linker and relocations | `Loaders/Linker/Linker.*` | Loads the main module and libraries, resolves NID-style imports against module/library exports, applies common x86-64 relocations, supplies unresolved-symbol trampolines, and links HLE exports. |
| Module and symbol records | `Loaders/Module.hpp`, `Loaders/Symbol.hpp` | Stores host base address, size, entry point, filename, module/library IDs, required/exported libraries, TLS, dynamic tables, segments, and NID-qualified symbols. |
| Kernel/sysmodule compatibility | `OS/HLE.hpp`, `OS/Libraries/`, `Loaders/App/AppLoader.cpp` | Builds an HLE module containing syscall/library stubs, loads configured system modules, and exposes PS4 library namespaces to the linker. |
| Threads and TLS | `OS/Thread.*`, `OS/Libraries/Kernel/` | Uses host pthreads, thread IDs, thread-local guest TLS state, and module TLS IDs. |
| Guest filesystem | `OS/Filesystem.*` | Maps guest devices such as `/app0`, `/dev`, `/temp0`, and `/system` to host paths and exposes file/directory operations. |
| Kernel objects and services | `OS/SceObj/`, `OS/Libraries/Kernel/`, `OS/UserManagement/`, `OS/Np/` | Provides the beginnings of PS4 object, kernel, user, and network-service compatibility. |
| GPU and graphics runtime | `GCN/` | Contains PM4 command processing, GCN command/compute jobs, shader decoding/decompilation, detiling, data formats, and renderer backends. |
| Host renderer | `GCN/Backends/`, `GCN/Renderer.*` | Defines renderer operations and includes a Vulkan-oriented implementation and GPU resource handling. |
| Platform/common layer | `Common/`, `Configuration.hpp`, CMake/dependency configuration | Supplies logging, helpers, configuration, filesystem conventions, and third-party integrations such as ELFIO, Xbyak, Zydis, SDL, Vulkan, and VMA. |

### CPU and execution architecture

The upstream source does not present a separate Jaguar CPU interpreter as its primary execution path. Its loader maps modules into host memory, stores host pointers for bases and entry points, and the application runtime eventually transfers control to the linked entry point. `Linker.cpp` also uses Xbyak-generated x86-64 trampolines for unresolved symbols, while Zydis is included among the upstream dependencies for instruction-level tooling.

This establishes the important architectural direction: PS4 user code is treated as x86-64 code and the execution boundary is intended to be host-native. It does not mean the upstream runtime is complete or that arbitrary commercial software is supported. The Xbox port should preserve the M8 synthetic executor for deterministic tests, but must not make it the permanent execution architecture.

### ELF loading and module linking

Upstream's ELF loader is more PS4-specific than the current Xbox loader. In addition to basic load segments, it extracts dynamic tags and tables, library/module descriptors, NID-qualified symbols, TLS information, process parameters, and exception-frame metadata. It also has a SELF input path and a separate code-patching stage.

The linker owns the application-wide view of modules. It can load the main executable, load additional libraries, build an HLE module, resolve imports, and apply relocations. The upstream `Module` record is consequently both a loaded image and a dynamic-linking record; it is not only a name/base/size entry.

### Kernel and sysmodule handling

The upstream application loader discovers the application and then loads a configured set of `.sprx` system modules. HLE supplies a synthetic module of library exports and stubs so unresolved PS4 APIs have a link target. The OS tree then implements compatibility surfaces in library-specific files.

The current Xbox ABI and syscall dispatcher cover the transition and smoke-test boundary, but they are not yet equivalent to upstream's NID/library namespace, HLE module, sysmodule catalog, filesystem mounts, or kernel-object model.

### GPU architecture

Upstream has a dedicated GCN runtime: command processing, PM4 packets, compute/graphics jobs, shader handling, detiling, and renderer backends. The current Xbox graphics code is a UWP/Xbox D3D12 host presentation layer. It does not implement PS4 GCN semantics. These should remain separate layers: an eventual GCN compatibility layer can target an Xbox D3D12 backend without replacing the existing host UI and presentation code.

### Memory and threading

Upstream currently uses host-backed mapped module memory and host pointers, with Windows virtual-memory protection applied around loaded segments. Its thread layer uses host threads and keeps guest TLS state per host thread.

The Xbox port's `GuestMemory` region abstraction is safer and more portable for tests: it explicitly tracks guest addresses, sizes, permissions, mapping, reads, writes, and zero-fill. It is a good foundation, but it needs a future host-address/guest-address translation policy before it can back native execution. The Xbox `Process`, `Thread`, and `Scheduler` abstractions are useful ownership and test models; their synthetic scheduling path should not be mistaken for the upstream native-thread runtime.

## Current Xbox/UWP component map

| Current component | Current role | Assessment against upstream |
|---|---|---|
| `Core/Loader/Elf64Loader.*` | Validates ELF64 headers/program headers and maps `PT_LOAD` data into `GuestMemory`, including permissions and zero-fill. | Reuse the validation, report, and memory-safety behavior. Extend or adapt it toward upstream-compatible dynamic metadata rather than creating another loader. SELF, SCE-specific program headers, TLS, process parameters, and dynamic tables are missing or incomplete. |
| `Core/Memory/GuestMemory.*` | Platform-neutral mapped-region model with bounds and permission checks. | Reuse as the test-safe guest memory contract. Add a later host-backed/native mapping adapter only when the execution boundary is designed. |
| `Core/Kernel/Process/*` | Owns process state, threads, the module manager, executable metadata, and symbol/dependency entry points. | Keep as the Xbox process façade. Add an upstream-style application/runtime composition behind it later; do not replace it with a second process abstraction. |
| `Core/Kernel/Thread/*`, `Core/Kernel/Scheduler/*` | M9 process/thread/scheduler model and synthetic execution integration. | Keep for tests and debugging. Later map native module starts, TLS, and ABI state into it; do not delete the M8/M9 path. |
| `Core/ABI/*` and `Core/Syscalls/*` | x86-64 calling-convention, syscall transition, dispatcher, PS4-style stubs, and ABI smoke tests. | Reuse as the syscall/ABI boundary. Extend it to receive NID-qualified library calls and HLE implementations; it is not a replacement for the upstream symbol/linker layer. |
| `Core/Loader/Module/*` | M11/M12 `LoadedModule`, registry, manager, dependency records, symbol lists, and load state. | Keep the API and tests as an adapter boundary. Its internals will eventually need upstream fields such as module/library IDs, NIDs, dynamic tables, TLS, init functions, and segment permissions. Avoid maintaining two independent module managers. |
| `Core/Loader/Relocations/*` | Safe relocation types, resolver interface, bounds checking, logging, and failure reporting. | Keep the safety contract. Align the type and symbol semantics with the upstream linker, and add actual dynamic-table inputs before adding more relocation cases. Unsupported types must continue to fail without writing memory. |
| `Core/Execution/*` | Platform-neutral execution context and backend interface; native backend is currently a boundary, not an arbitrary host-code executor. | Keep as the intended seam for upstream-compatible native x86-64 execution. The synthetic M8 backend remains a test backend. The native backend should be implemented only after mapping, relocation, ABI, TLS, and exception policy are specified. |
| `platform/xbox/ChonkyStation4.Xbox/Graphics/*` | Xbox/UWP D3D12 host rendering and presentation. | Keep Xbox-specific. It is not the PS4 GCN implementation. Use a future GCN-to-D3D12 adapter if and when GPU work is authorized. |
| `platform/xbox/ChonkyStation4.Xbox/Host/*` and XAML | UWP storage, input, diagnostics, and debug buttons. | Keep Xbox-specific. Provide the host services required by the common loader/OS interfaces instead of importing the desktop SDL/Vulkan entry point. |

## Reuse, replacement, and missing pieces

### Reuse directly

- `GuestMemory`'s explicit mapping and bounds/permission checks.
- The current process, thread, ABI, syscall, smoke-test, and Xbox UI boundaries.
- The relocation framework's no-corruption failure behavior and diagnostics.
- The Xbox D3D12 host/presentation and UWP storage/input integration.
- The M8 synthetic CPU executor as a deterministic test/debug backend.

### Replace or adapt internally over time

- Treat the current `ModuleManager` as the compatibility façade, then make its loaded-module data model represent upstream concepts: NID, module ID, library ID, dynamic tables, TLS, init/fini functions, and segment metadata.
- Adapt `Elf64Loader` to expose the dynamic information required by a linker. Reuse its safe mapping path instead of copying upstream's direct pointer writes into the Xbox test path.
- Adapt `SymbolResolver` from plain names to a qualified lookup model that can represent symbol name/NID, library, module, version, binding, and import/export state.
- Connect the ABI/syscall dispatcher to an HLE/sysmodule registry. This is the bridge between a resolved library symbol and a callable compatibility implementation.
- Keep `IExecutionBackend` as the boundary, but replace the current no-op/native placeholder only after the loader/linker and process startup contracts are complete.

### Missing relative to upstream

1. SELF parsing/decryption integration and PS4-specific program-header handling.
2. `PT_DYNAMIC`/dynamic-string/symbol/relocation parsing integrated with `LoadedModule`.
3. SFO/application metadata and `eboot.bin`/`sce_module` package discovery.
4. Linker-owned ordered module graph, NID-qualified symbol lookup, init/fini sequencing, and TLS image setup.
5. HLE module construction, sysmodule discovery, and the upstream OS library namespace.
6. Guest filesystem device mounts and a UWP-safe equivalent of `/app0`, `/dev`, `/temp0`, and `/system`.
7. Native x86-64 execution handoff with a defined host/guest address, stack, ABI, exception, and security policy.
8. PS4 GCN command processing, PM4, shader, detiling, GPU memory, and renderer compatibility.
9. Broader kernel objects, user management, NP/PSN services, and title-specific patches.
10. A platform abstraction that can host the common runtime on UWP without assuming SDL, desktop paths, or Vulkan.

## Recommended porting strategy

### Phase 0 — Freeze and define the adapter boundary

Keep all M7–M12 tests and the existing Xbox UI. Do not add a new generic emulator subsystem while the upstream model is being mapped. Make `Process`, `ModuleManager`, `GuestMemory`, ABI, and `IExecutionBackend` the explicit Xbox integration façade.

### Phase 1 — Upstream-compatible data model

Design one common module record, based on the intersection of the current `LoadedModule` and upstream `Module`. It should represent load segments, guest/base addresses, dynamic tables, TLS, module/library identities, NID-qualified symbols, dependencies, relocation ranges, and initialization functions. Preserve the current public test APIs through adapters rather than duplicating records.

### Phase 2 — Loader/linker adapter

Extend the existing loader report to expose dynamic information and PS4-specific segment metadata. Add package/SFO discovery as a separate application-loader layer. Then implement linker ordering and symbol resolution on top of the module registry. Relocation writes must go through checked memory access and must report unsupported types; upstream's direct pointer writes and panic-on-unsupported behavior should not be copied into the Xbox safety path.

### Phase 3 — HLE and sysmodule bridge

Represent HLE exports and loaded `.sprx` modules in the same registry. Map qualified upstream library symbols to the existing ABI/syscall dispatcher, with explicit missing-symbol and unsupported-service results. Only after this bridge works should system-library loading become part of process startup.

### Phase 4 — Native execution boundary

Define the native backend contract for host x86-64 execution: module mapping, entry address, stack, register/ABI state, TLS, exception routing, and process/thread lifetime. Keep synthetic execution selectable for tests. This phase must not silently execute untrusted arbitrary host pointers; validation and opt-in policy are part of the backend design.

### Phase 5 — GCN and host graphics

Port the upstream GCN concepts behind a platform-neutral GPU interface. Keep the Xbox D3D12 renderer and UWP presentation layer as the host implementation. Vulkan/SDL assumptions from the desktop project must stay out of the UWP-facing layer.

### Phase 6 — Services and compatibility breadth

Add filesystem mounts, kernel objects, user management, NP/PSN services, and title-specific compatibility only as individual, testable service families. These are runtime-environment work, not reasons to broaden the CPU emulator.

## Validation gates

Each phase should preserve:

- M7 ELF and guest-memory tests.
- M8 synthetic CPU and exception tests.
- M9 process, thread, scheduler, and runtime tests.
- M10 ABI and syscall tests.
- M11 module/execution-boundary tests.
- M12 dependency, symbol, and safe-relocation tests.
- Xbox project registration, debug UI, Release compilation, and MSIX generation.

The audit itself does not claim that real PS4 games can execute. The next implementation milestone should be a narrowly scoped **M13: upstream-compatible dynamic ELF/linker adapter design and test fixture**, not a new CPU emulator or an unbounded sysmodule port.

