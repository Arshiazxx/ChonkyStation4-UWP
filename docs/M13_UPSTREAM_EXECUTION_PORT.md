# M13 — Upstream Execution Source Port

Status: upstream execution boundary and Xbox/UWP adaptation layer integrated. Native PS4 entry transfer remains blocked. This milestone does not claim that PS4 user code executes.

## Authority and source audit

The upstream source tree is retained in this workspace under `ChonkyStation4/` and is the authority for the execution sequence. The corresponding upstream repository is:

<https://github.com/liuk7071/ChonkyStation4>

The execution-related upstream files audited for M13 are:

| Upstream source | Role in the execution path | M13 treatment |
| --- | --- | --- |
| `Loaders/ELF/ELFLoader.hpp/.cpp` | Parses ELF/SELF, maps PT_LOAD data, records dynamic metadata, TLS, entry, and segments | Existing `Core/Loader` remains the safe Xbox-facing loader. Upstream metadata and host mapping requirements are represented at the new backend boundary; raw desktop loader code is not copied into the UWP project. |
| `Loaders/Linker/Linker.hpp/.cpp` | Loads libraries, resolves NID imports/exports, applies relocations, generates unresolved-symbol trampolines | Existing M11/M12 dependency, symbol, and bounds-checked relocation layers remain in use. Upstream linker sequencing is documented as the next adapter input. |
| `Loaders/App/AppLoader.cpp` | Discovers `eboot.bin`, package metadata, sysmodules, and `.sprx` files | Xbox path handling is supplied through the UWP host/application-data seam. Desktop package and mount assumptions are not imported unchanged. |
| `Loaders/App.cpp` | Initializes modules and prepares the PS4 entry stack/register state before jumping to `Module::entry` | M13 ports the entry-state concept into `Upstream::EntryState`; the final native bridge is still gated. |
| `Loaders/Module.hpp` and `Loaders/Symbol.hpp` | Stores host base, entry, TLS, segment, export, import, and symbol metadata | Existing `Core/Loader/LoadedModule` remains the process-owned model. A host native entry pointer is now an explicit separate field in `ExecutionContext`; a guest address is never cast to a host pointer. |
| `OS/Thread.hpp/.cpp` | Creates host threads, initializes guest TLS, and invokes the entry function | M13 adds a `std::thread`/`thread_local` platform seam. Upstream private TLS and inline-assembly assumptions are not copied. |
| `OS/HLE.hpp/.cpp` | Builds HLE exports and symbol stubs required by linking | Existing ABI/syscall/kernel layers remain the Xbox compatibility surface. The full upstream HLE library set is future work. |
| `Loaders/ELF/CodePatcher.cpp` | Uses Zydis for x86-64 inspection and Xbyak for TLS/red-zone patch code | Not imported into the Xbox target. Generated executable code requires a separate capability and policy pass. |
| `Common/Common.hpp` | Defines common types and the upstream `PS4_FUNC` SysV ABI annotation | The ABI requirement is recorded, but MSVC x64 cannot consume the upstream inline-assembly/calling-convention path unchanged. |
| `PlayStation4.cpp` and `ChonkyStation4.cpp` | Desktop runtime orchestration, graphics, filesystem mounts, and CLI bootstrap | Remains desktop-only reference code. The UWP app entry and D3D12 host stay Xbox-specific. |

## Ported M13 architecture

The new backend path is:

```text
Core::Execution::UpstreamExecutionBackend
    ↓ validates x86-64 ExecutionContext
    ↓ builds Upstream::EntryState
Core::Execution::Upstream::XboxUwpExecutionPlatform
    ├── executable-memory API seam
    ├── memory-protection API seam
    ├── guest TLS thread-local seam
    ├── host-thread seam
    ├── UWP application-data root seam
    └── native entry-transfer capability gate
```

The compatibility class `UpstreamCompatibleX64Backend` now delegates to `UpstreamExecutionBackend`, preserving existing M11/M12 callers while replacing the old standalone placeholder logic.

`SyntheticTestBackend` is unchanged and remains the only backend that executes the private M8 test encoding.

## Xbox/UWP adaptations

### Executable memory and protection

`XboxUwpExecutionPlatform` exposes explicit allocation, protection, and release operations. The Xbox project selects `VirtualAllocFromApp` and `VirtualProtectFromApp` through `CHONKYSTATION4_XBOX_UWP`. Desktop Core validation uses the ordinary Win32 fallback so the platform adapter can be tested without linking UWP-only symbols.

The backend does not allocate or execute generated code during normal validation. `PlatformCapabilities::executableMemory`, `memoryProtection`, and `nativeEntryTransfer` remain false until Xbox policy and runtime behavior are proven.

### TLS

The adapter provides a thread-local guest TLS pointer seam. This is intentionally simpler than upstream's private Windows TLS layout and `_tls_index`/`gs` access. The upstream TLS image, module IDs, TCB layout, and access patching still require a dedicated port.

### Host threads

The adapter provides a joining `std::thread` seam corresponding to upstream `OS::Thread::createThread`/`joinThread`. It does not call guest code and does not replace the M9 process/thread model.

### Filesystem paths

The UWP host can inject its `ApplicationData::Current->LocalFolder->Path` through `SetApplicationDataRoot`. The Core layer does not call WinRT directly. This keeps UWP path policy in the host while giving future AppLoader adapters a stable root.

### Platform services

Desktop SDL/Vulkan startup, filesystem mounts, VSH initialization, graphics services, and the upstream CLI bootstrap are not imported into the UWP execution backend. D3D12 and UWP application lifecycle remain in the existing Xbox host.

## Native entry status

The backend now performs the upstream-style preparation steps:

1. Verify that the host is x86-64.
2. Verify the process/thread execution context.
3. Capture the explicit host native entry pointer, stack pointer, parameter block, exit handler, and main-module name.
4. Route the state to the platform entry-transfer adapter.

The transfer is currently rejected when no host native entry pointer is installed. Even if one is supplied, the adapter rejects it until all of these contracts are implemented and tested:

- PS4 SysV register and stack setup;
- an Xbox-compatible bridge for MSVC x64, which has no inline x64 assembly;
- host executable-page policy and instruction-cache handling;
- guest TLS image/TCB initialization;
- imported-symbol/HLE trampoline calls;
- exception and unwind behavior;
- safe module mapping from the Core guest model to host executable memory.

Consequently, M13 does not claim real PS4 entry execution.

## Blocked upstream APIs and dependencies

The following upstream mechanisms remain blocked or unported:

- direct host-pointer ELF/SELF mapping from upstream `ELFLoader`;
- upstream's desktop `VirtualAlloc`/`VirtualProtect` assumptions as a complete mapping policy;
- `pthread-win32` host-thread dependency;
- private TLS access through `_tls_index` and `gs` assembly;
- inline x86-64 stack alignment and `jmp *entry` assembly;
- Xbyak generated trampolines and Zydis code patching;
- generated executable buffers under Xbox/UWP policy;
- complete PS4 NID database and HLE/sysmodule implementation;
- desktop filesystem mounts, SDL/Vulkan/VSH initialization, and CLI startup;
- upstream direct relocation writes without the Core bounds/safety layer.

These are compatibility tasks, not reasons to expand or replace the synthetic M8 ISA.

## Validation output

The new `UpstreamExecutionSmokeTests` validates the real Core backend boundary and expects:

```text
ChonkyStation4 Upstream Execution Test

Architecture:
x86-64

Upstream source adapter:
PASS

Xbox/UWP platform layer:
PASS

Native PS4 entry transfer:
BLOCKED

Result:
SUCCESS
```

`SUCCESS` means the adapter rejected unsafe execution cleanly. It does not mean a PS4 ELF reached a native entry point.

## Next port phase

M14 should port a controlled upstream-compatible entry fixture, not a commercial game:

1. map a known x86-64 test image into a validated host executable region;
2. implement and test the Xbox ABI bridge;
3. initialize a real guest TLS image on a host thread;
4. connect imported symbols to the existing ABI/HLE boundary;
5. validate exceptions, return state, and cleanup;
6. only then consider enabling `nativeEntryTransfer` for a controlled fixture.

The synthetic backend must remain unchanged throughout this work.
