# M1 — ChonkyStation4 Architecture Audit

**Status target:** Architecture analysis only  
**Authoritative upstream:** `liuk7071/ChonkyStation4`  
**Verified source commit:** `310269290a3c256f5911d4bc7e441489bffffbf6`  
**M0 migration notes:** `MIGRATION_NOTES.md`  
**Xbox reference inventory:** `docs/migration/OLD_XBOX_PORT_REFERENCE.md`

No emulator implementation code, Xbox implementation, UWP project, or D3D12 code was added in M1.

---

## 1. Repository architecture

### 1.1 Top-level responsibilities

| Area | Current files / directories | Role |
|---|---|---|
| Desktop frontend | `ChonkyStation4/ChonkyStation4.cpp` | CLI, early VA reservation, user selection, paths, start command |
| Runtime orchestration | `PlayStation4.*`, `Loaders/App.*` | load/run sequencing, guest main thread, pre-entry initialization |
| Game/app loading | `Loaders/App/AppLoader.*` | game directory, SFO, eboot, sysmodules/LLE selection, mounts |
| SELF/ELF loading | `Loaders/ELF/*` | SELF extraction, ELF parse, direct VA placement, TLS, code patches |
| Dynamic linking | `Loaders/Linker/*`, `Loaders/Module.hpp`, `Loaders/Symbol.hpp` | relocations, HLE/LLE symbol resolution, Xbyak stubs |
| HLE/kernel | `OS/HLE.*`, `OS/Libraries/Kernel/**`, `OS/SceObj.*` | symbol exports, kernel object model, memory, pthread, events, files |
| PS4 services | `OS/Libraries/Sce*/**` | video/GNM, pad, audio, network, save data, user, RTC, etc. |
| Filesystem | `OS/Filesystem.*` | guest device/path mapping backed by host files/directories |
| Thread runtime | `OS/Thread.*`, `OS/Libraries/Kernel/pthread/**` | guest thread creation, TLS, host pthread forwarding |
| GPU frontend | `GCN/GCN.*`, `GCN/CommandProcessor.*`, `GCN/PM4.hpp` | queues, PM4 decode, emulated GPU state/commands |
| GPU backend API | `GCN/Backends/Renderer.hpp` | partial renderer virtual interface + shared GCN register helpers |
| Vulkan renderer | `GCN/Backends/Vulkan/**` | Vulkan device/swapchain/resources/pipelines/caches/presentation |
| Shader frontend | `GCN/Shader/**`, `GCN/FetchShader.*` | GCN decode and decompile, resource metadata |
| Vulkan shader compiler | `Vulkan/ShaderCache.*`, `Vulkan/GLSLCompiler.hpp` | GLSL -> glslang -> SPIR-V -> VkShaderModule |
| PSN/network providers | `PSN/**`, `SceNet`, `SceNpWebApi` | Asio/cpp-httplib based host networking |
| Build | `CMakeLists.txt`, `.gitmodules` | one desktop executable; submodule dependencies; Vulkan mandatory |

### 1.2 CPU subsystem / native execution runtime

There is no standalone CPU interpreter directory and no conventional guest-ISA-to-host-ISA JIT translator. PS4 guest CPU code is x86-64 and is executed natively after loading, linking and patching.

Key path:

```text
ELFLoader
  -> commits guest module into host VA
  -> copies ELF segments
  -> records TLS
  -> patches selected guest instructions
Linker
  -> applies x86-64 ELF relocations
  -> points imports at HLE/LLE/native stubs
App::run
  -> creates guest main host thread
initAndJumpToEntry
  -> initializes runtime/GPU/filesystem/modules
  -> x86-64 assembly jump to guest entry
```

Runtime code generation exists for:

- unresolved symbol trampolines (`Linker.cpp`, Xbyak)
- HLE symbol stubs (`Module.hpp`, Xbyak)
- TLS-access patches and game-specific red-zone patches (`CodePatcher.cpp`, Xbyak + Zydis)

Classification: **native-execution runtime with dynamic code generation**, not a portable JIT backend.

### 1.3 Memory manager / virtual memory system

Memory is distributed across startup, loader, kernel memory APIs, code patching, and Vulkan dirty tracking rather than encapsulated in a single memory manager.

Primary components:

- `ChonkyStation4.cpp`: early 2 TiB reserve at `0x80000000` on Windows.
- `ELFLoader.cpp`: scans reserved VA, commits module memory, protects loaded code.
- `Kernel.cpp`: direct/flexible memory, reserve/map/query/unmap/mmap APIs.
- `CodePatcher.cpp`: near-code executable commits.
- `Vulkan/BufferCache.cpp`: page protection + vectored exception write tracking.
- `Vulkan/Pipeline.cpp`: checks host committed pages with `VirtualQuery` before GPU data access.

There is no backend-neutral VM service in current upstream.

### 1.4 Kernel emulation and syscall layer

`OS/HLE.cpp::buildHLEModule()` constructs a synthetic `Module` and registers host function pointers for `libkernel` and many `libSce*` modules. The dynamic linker then resolves guest imports against these exports.

No central numeric syscall-instruction dispatcher was found in the source tree. The practical “syscall layer” is primarily **HLE symbol dispatch through ELF relocations**:

```text
guest imported symbol
      |
ELF relocation / NID resolution
      |
HLE Module export
      |
host C++ PS4_FUNC implementation
```

`OS/SceObj.*` supplies a shared handle/object registry. Kernel event queues, event flags, semaphores and synchronization primitives live under `OS/Libraries/Kernel`.

### 1.5 Loader and SELF/ELF handling

- `AppLoader` handles installed-game directory layout, `param.sfo`, eboot path, sysmodules and guest mounts.
- `ELFLoader` accepts ELF and PS4 SELF container data, parses ELF metadata through ELFIO, maps PT_LOAD segments directly into host VA, tracks PT_TLS, and records dynamic relocation structures.
- `Linker` builds HLE module(s), applies x86-64 relocations and resolves HLE/LLE imports.
- LLE modules are loaded through the same loader/linker path.

This parsing/linking logic is primarily emulator core logic. The host-specific portion is **where and how bytes are mapped into virtual memory and how host files are opened**.

### 1.6 GPU emulation

The guest GPU frontend is organized around:

- `SceGnmDriver`: receives PS4 GNM command submissions.
- `GCN::submitGraphics/submitCompute/submitFlip`: queues work.
- `GCN::gcnThread`: serial renderer command loop plus async compute coroutine path.
- `CommandProcessor`: decodes PM4 command buffers and updates renderer registers/state.
- `Renderer`: backend operations invoked by the command processor.

This is the correct high-level insertion point for a second backend.

### 1.7 Renderer abstraction

`Renderer.hpp` is a useful but incomplete abstraction.

Backend-neutral aspects:

- draw/indirect draw
- compute dispatch
- flip
- GDS fill
- shared GCN register/state decoding helpers

Backend/host leakage:

- includes SDL
- owns `SDL_Window*`
- `GCN.hpp` directly includes and constructs `VulkanRenderer`

Therefore M4 should preserve `CommandProcessor` and refine the renderer creation/window boundary rather than rewrite GCN command emulation.

### 1.8 Shader pipeline

```text
GCN machine code
  -> Shader::Decoder / Instruction
  -> ShaderDecompiler
  -> ShaderData
       |- generated source string
       |- buffer/descriptor metadata
       `- vertex output metadata
  -> Vulkan ShaderCache
  -> compileGLSL()
  -> glslang
  -> SPIR-V
  -> VkShaderModule
```

The decoder/decompiler semantics are reusable. GLSL/Vulkan/SPIR-V compilation is backend-specific.

### 1.9 Audio

`SceAudioOut` combines guest PS4 audio-port behavior and host SDL audio device management. It opens one SDL playback device, queues decoded/raw guest PCM, and uses SDL queued-audio size for backpressure.

Boundary recommendation: retain guest port/format semantics; abstract host PCM sink/device operations.

### 1.10 Input

`ScePad` combines PS4 pad state translation with direct SDL keyboard/game-controller polling, LEDs and rumble. Hotplug event handling is currently in `VulkanRenderer::flip()`.

Boundary recommendation: retain `ScePadData` and PS4 semantics; feed it from a host input source independent of the graphics backend.

### 1.11 Filesystem

`PS4::FS` already models PS4 guest device mounts and file IDs, but backing is host-specific:

- `std::filesystem`
- `FILE*`
- CRT seeking/read/write
- direct filesystem iteration
- kernel `mmap` extracts a Win32 file handle from `FILE*`

The guest namespace is reusable. The host file/storage layer requires abstraction for UWP.

### 1.12 Networking

`SceNet` uses standalone Asio for TCP/UDP sockets and resolver operations; Windows build links `Ws2_32`. PSN provider code also uses Asio, and `SceNpWebApi` uses cpp-httplib.

The PS4-facing network semantics should remain. Host transport/capability behavior needs UWP review and M5 runtime validation.

### 1.13 Threading and synchronization

**Portable internal synchronization:**

- `std::thread`
- `std::mutex`
- `std::condition_variable`
- `std::counting_semaphore`
- atomics
- `std::chrono`

**Non-portable guest thread runtime:**

- pthread-win32 types and functions
- guest pthread APIs forwarding directly to host pthread
- Windows `SetThreadDescription`
- Windows `GetCurrentThreadStackLimits`
- Windows `TerminateThread`
- `_tls_index` and `gs:[0x58]` host TLS assumptions

`cppco` is also used for asynchronous GPU compute coroutine switching and must be verified for the Xbox toolchain/ABI, though it is not presently a Win32 service API.

### 1.14 Frontend/application layer

Current application hosting is desktop-centric:

- standard `main(argc, argv)`
- CLI11 commands/options
- SDL preference directory
- SDL window created by Vulkan renderer
- renderer owns window event pump
- process termination in several code paths uses `_Exit`/`exit`

A UWP host should be a separate frontend that invokes emulator bootstrap services, not a conversion of the CLI `main()` into platform code scattered across the core.

### 1.15 Build system and dependencies

Current `CMakeLists.txt` creates **one** executable and unconditionally includes the Vulkan stack. Windows adds pthread-win32 and `Ws2_32` and specifies unusual image/ASLR/stack linker settings.

Important build behavior:

- C++23
- `SDL_MAIN_HANDLED`
- Windows `/DYNAMICBASE:NO`
- executable image base `0x700000000000`
- 2 MiB configured stack
- Vulkan required unconditionally
- interprocedural optimization enabled

Dependency audit:

| Dependency | Current role | Xbox architectural classification |
|---|---|---|
| CLI11 | desktop CLI | REPLACEABLE |
| SDL | window, input, audio, paths, timing | REQUIRES ABSTRACTION |
| Vulkan | graphics backend/presentation | REPLACEABLE |
| VulkanMemoryAllocator | Vulkan resource allocation | REPLACEABLE |
| glslang/SPIR-V | Vulkan shader compilation | REPLACEABLE for D3D12 path |
| Zydis | x86-64 decoding for guest code patches | PORTABLE library; runtime use tied to M7 |
| Xbyak | runtime x86-64 code generation | XBOX BLOCKER until executable-code capability is proven |
| ELFIO | ELF parsing | PORTABLE |
| pthread-win32 | guest pthread backing | REQUIRES ABSTRACTION / potential XBOX BLOCKER |
| Asio | socket/resolver transport | REQUIRES ABSTRACTION / capability validation |
| cpp-httplib | web API | REQUIRES ABSTRACTION / capability validation |
| cppco | async compute coroutine switching | INVESTIGATE toolchain/ABI compatibility |
| xxHash | shader/cache hashing | PORTABLE |
| miniz | compression | PORTABLE |
| TinySHA1 | hashing | PORTABLE |
| Dolphin utility headers | utility/bitfield code | PORTABLE where used |
| NVIDIA Aftermath | optional desktop GPU diagnostics | DROP for Xbox target |

---

## 2. Actual execution flow

The requested conceptual order is useful, but the **actual upstream order is more interleaved** because loader memory mapping occurs before the guest main thread and GPU initialization occurs on that guest host thread before the final jump.

```text
Host process launch
  |
  |-- Windows: reserve 2 TiB VA @ 0x80000000 (best effort)
  |-- parse CLI / initialize user paths
  v
PS4::loadAndRun(path)
  |
  |-- OS::Thread::init()
  |     `-- derive host TLS offset for guest TLS pointer
  |
  |-- choose loader path
  |     |-- directory -> AppLoader
  |     `-- ELF/SELF -> Linker::loadAndLink
  |             |-- ELFLoader maps module memory
  |             |-- build HLE Module
  |             |-- resolve relocations
  |             `-- generate unresolved stubs as needed
  |
  |-- optional sysmodule/LLE loading + relocation
  |-- game-specific patches
  v
App::run()
  |
  `-- OS::Thread::createThread("main")
        |
        v
initAndJumpToEntry()
  |
  |-- PS4::init()
  |     |-- detached GCN thread
  |     |     |-- VulkanRenderer::init()
  |     |     |-- GCN command processor init
  |     |     `-- renderer ready flag
  |     |
  |     |-- mount DEV/TEMP0/SYSTEM/SYSTEM_EX
  |     |-- FS::init()
  |     `-- wait for GCN initialized
  |
  |-- run loaded module initializers
  |-- libc runtime initialization
  v
native x86-64 jump to guest entry
  |
  |-- guest imports call HLE/LLE function pointers
  |-- guest GNM submits PM4 command buffers
  |-- guest VideoOut requests flips
  |-- guest Pad/Audio/FS/Net call host-backed HLE
  v
GCN thread
  |-- PM4 CommandProcessor
  |-- Renderer draw/dispatch
  `-- VulkanRenderer flip -> present + SDL events + pad polling
```

### Host-OS touch points in execution flow

| Stage | Direct host dependency |
|---|---|
| Process start | Win32 VirtualAlloc reserve; CLI; SDL pref path |
| Thread runtime init | Windows TLS internals |
| Module loading | filesystem + Win32 VM |
| Relocation/stubs | Xbyak executable code |
| Guest thread creation | pthread-win32 + Win32 stack/thread APIs |
| PS4 init | std::thread, filesystem, host paths |
| Renderer init | SDL + Vulkan + GPU drivers |
| GPU cache | Win32 page protection + vectored exceptions |
| Input | SDL keyboard/controller |
| Audio | SDL audio |
| Networking | Asio/Winsock-backed transport |
| Guest file I/O | std::filesystem + CRT; Win32 file mapping for mmap |

---

## 3. Platform dependency audit

Classification meanings:

- **PORTABLE** — usable on Xbox/UWP without architectural replacement, subject to normal toolchain support.
- **REPLACEABLE** — implementation is desktop/backend-specific but already sits at a sensible replacement point.
- **REQUIRES ABSTRACTION** — host API is embedded in emulator-facing code and needs a clean boundary.
- **XBOX BLOCKER** — current execution fundamentally depends on behavior that must be proven or redesigned before the emulator can execute on Xbox.

### 3.1 Windows APIs

| Dependency | Files / use | Classification | Notes |
|---|---|---|---|
| `VirtualAlloc/VirtualQuery/VirtualFree` | startup, ELF loader, kernel, code patcher | **XBOX BLOCKER** | fixed large VA model and near allocations are fundamental; M5/M6 gate |
| `VirtualProtect` executable/RW protections | ELF loader, code patcher | **XBOX BLOCKER** | dynamic executable memory is M7 gate |
| `VirtualProtect` read-only dirty tracking | Vulkan BufferCache | **REQUIRES ABSTRACTION** | D3D12 still needs guest-write coherence mechanism |
| `AddVectoredExceptionHandler` | Vulkan BufferCache | **REQUIRES ABSTRACTION** | runtime availability must be tested; alternative may be needed |
| `CreateFileMappingW/MapViewOfFile` | kernel file mmap | **REQUIRES ABSTRACTION** | crossing FS/VM layers; UWP support must be validated |
| `GetSystemInfo` | allocator/cache page size | **REPLACEABLE** | host page-size query service |
| `SetThreadDescription` | thread/GCN/logging | **REPLACEABLE** | diagnostic only |
| `GetCurrentThreadStackLimits` | guest thread metadata | **REQUIRES ABSTRACTION** | stack ABI/guest pthread behavior depends on it |
| `TerminateThread` | guest pthread exit | **REQUIRES ABSTRACTION** | unsafe desktop-specific implementation detail |
| Windows TLS `_tls_index`, `gs:[0x58]` | `OS/Thread`, CodePatcher | **XBOX BLOCKER** | native guest TLS scheme; M7 |
| `_get_osfhandle/_fileno` | kernel mmap | **REQUIRES ABSTRACTION** | CRT-to-Win32 handle leakage |

### 3.2 POSIX/pthread APIs

| Dependency | Classification | Notes |
|---|---|---|
| pthread create/join/attrs | **REQUIRES ABSTRACTION** | backing guest threads directly with pthread-win32 |
| pthread mutex/cond/rwlock | **REQUIRES ABSTRACTION** | guest-visible pthread object representation is host-coupled |
| pthread TLS-related guest functions | **REQUIRES ABSTRACTION** | several are stubbed/custom; should not assume UWP pthread library works |
| std mutex/condition/semaphore | **PORTABLE** | keep for emulator-internal synchronization |

### 3.3 SDL

| Current SDL use | Classification | Recommended boundary |
|---|---|---|
| window creation / Vulkan surface | **REPLACEABLE** | graphics host/presentation |
| event pump / quit/fullscreen | **REQUIRES ABSTRACTION** | application lifecycle/window host |
| controller/keyboard/rumble/LED | **REQUIRES ABSTRACTION** | input service |
| audio device / queue | **REQUIRES ABSTRACTION** | audio sink |
| preference path | **REQUIRES ABSTRACTION** | app-data/storage service |
| performance counters/ticks/delay | **REPLACEABLE** | monotonic clock / sleep |

### 3.4 Vulkan

| Area | Classification |
|---|---|
| Vulkan instance/device/swapchain | **REPLACEABLE** |
| Vulkan pipeline/cache/resources | **REPLACEABLE** |
| VMA | **REPLACEABLE** |
| Vulkan synchronization/presentation | **REPLACEABLE** |
| GCN command processor feeding renderer | **PORTABLE** |
| GCN register/state helpers in Renderer | **PORTABLE**, though SDL member must be removed/refined later |

For Xbox the current mandatory Vulkan build path is a target build blocker, but it is not evidence that the PS4 GPU emulation itself must be replaced.

### 3.5 Filesystem APIs

| API style | Classification | Notes |
|---|---|---|
| guest path/device translation | **PORTABLE** | valuable core behavior |
| `std::filesystem` direct host roots | **REQUIRES ABSTRACTION** | UWP storage/root policy differs |
| `FILE*` backing and CRT I/O | **REQUIRES ABSTRACTION** | host representation leaks into VM mmap |
| app/sysmodule path discovery | **REQUIRES ABSTRACTION** | currently SDL-pref-path based |

### 3.6 Threading / synchronization APIs

| Area | Classification |
|---|---|
| internal `std::thread` | **PORTABLE** |
| mutex/atomic/semaphore/chrono | **PORTABLE** |
| guest pthread forwarding | **REQUIRES ABSTRACTION** |
| host TLS internals | **XBOX BLOCKER** |
| cppco context switching | **INVESTIGATE** (treated as **REQUIRES ABSTRACTION** only if Xbox toolchain/runtime proves incompatible) |

### 3.7 Networking

| Area | Classification | Notes |
|---|---|---|
| PS4 SceNet object/API model | **PORTABLE** | guest semantics |
| standalone Asio transport | **REQUIRES ABSTRACTION** | UWP socket/capability behavior must be verified |
| explicit `Ws2_32` desktop link | **REPLACEABLE** | target-specific build choice |
| cpp-httplib PSN/Web API | **REQUIRES ABSTRACTION** | UWP networking/cert/runtime validation |

### 3.8 Audio

| Area | Classification |
|---|---|
| PS4 AudioOut port/format semantics | **PORTABLE** |
| SDL playback | **REQUIRES ABSTRACTION** |
| busy/backpressure policy around SDL queued bytes | **REPLACEABLE** |

---

## 4. Graphics architecture audit

### 4.1 Command submission

Guest-facing command submission originates in `SceGnmDriver` and enters `GCN` queues. `GCN::gcnThread()` drains commands and invokes `processCommands()` for graphics, or the async compute coroutine path for compute. `SceVideoOut` submits flip commands.

`CommandProcessor` is therefore part of PS4 GPU emulation, not host graphics API code.

### 4.2 Existing graphics abstraction

`Renderer` is the primary interface. `CommandProcessor` communicates through it and common GCN register state. This is the correct D3D12 insertion point.

However, three leaks must eventually be addressed:

1. `Renderer.hpp` directly includes SDL and stores `SDL_Window*`.
2. `GCN.hpp::initVulkan()` directly constructs `VulkanRenderer`.
3. `VulkanRenderer::flip()` contains generic host event/input/timing/UI work.

None were changed in M1.

### 4.3 Vulkan backend responsibilities

The backend currently owns:

- SDL initialization/window
- Vulkan instance/surface/device selection
- feature/extension checks
- graphics/present queue selection
- swapchain/image views
- command pool/buffers
- semaphores/fences
- VMA allocators/pools
- GDS buffer
- render targets/depth targets
- texture cache
- buffer cache
- pipeline and compute-pipeline caches
- descriptor construction/push descriptors
- shader modules
- flip blit/presentation
- SDL event handling and pad polling

D3D12 must be a sibling implementation of equivalent renderer responsibilities, while lifecycle/input/window ownership should move outside backend-specific drawing code.

### 4.4 Vulkan feature assumptions

Current initialization requires Vulkan 1.3 and checks several modern features/extensions, including dynamic rendering and extended dynamic state, plus geometry/tessellation/depth/fragment capabilities and extension-specific behavior. These Vulkan requirements should **not** be mechanically translated into D3D12 extension names. M4 must derive the D3D12 feature needs from actual renderer operations and PS4 emulation requirements.

### 4.5 Resource management

Vulkan-specific resource management is under:

- `BufferCache.*`
- `TextureCache.*`
- `RenderTarget.*`
- `Pipeline.*`
- `PipelineCache.*`
- `ComputePipeline.cpp`
- `VulkanRenderer.*`

VMA pools currently use large allocation block sizes. Those values should not be copied into an Xbox backend without M5 memory/resource-limit results.

### 4.6 Shader pipeline and D3D12 insertion

The GCN frontend produces GLSL-oriented `ShaderData::source`. Vulkan then compiles it with glslang to SPIR-V.

Future D3D12 integration choices belong to M4, but the architectural seam is clear:

```text
GCN decode / semantic decompile
              |
     backend-consumable shader model
          /                 \
Vulkan source/compiler    D3D12 source/compiler
       |                        |
    SPIR-V                     DXIL
```

M1 does **not** choose whether this requires a backend-neutral IR, an HLSL emitter, SPIR-V translation, or another strategy. That decision requires implementation-level shader review in M4.

### 4.7 GPU/host-memory cross-coupling

`BufferCache` marks guest host-memory pages read-only and uses a vectored access-violation handler to detect writes. This mechanism is not inherently Vulkan-specific even though it lives in the Vulkan directory. A later refactor should place the dirty-page tracking primitive behind host memory/caching services so a D3D12 backend can reuse equivalent semantics if Xbox supports them.

---

## 5. Memory architecture audit

### 5.1 Address ranges and reservation

Observed current upstream values:

| Purpose | Address/range |
|---|---|
| Early Windows reservation base | `0x0000000080000000` |
| Early reservation size | `2048 GiB` |
| General user mapping search | `0x80000000` to `0x80000000 + 2000 GiB` |
| ELF module placement start | `0x0000008000000000` (512 GiB) |
| System mapping search base | `0x001000000000` (64 GiB) |
| Guest reported page size | 16 KiB |

These values expose a deliberate direct-address-space design. UWP/Xbox cannot be treated as a normal heap-only port.

### 5.2 Reservation strategy

Startup reserves a very large PAGE_NOACCESS region. Subsequent allocators search with `VirtualQuery` for `MEM_RESERVE` regions and commit subranges. This relies on reservation state remaining queryable and subranges being commit-able at deterministic addresses.

### 5.3 Mapping strategy

- ELF segments: direct host pointers at module base + guest ELF VA.
- Direct/flexible memory: guest kernel APIs choose/commit host VA from reserved ranges.
- Fixed mappings: callers can require the returned address to equal the requested pointer.
- File-backed mmap: current Windows code maps a native host file independently via Win32 file mappings.

### 5.4 Protection handling

Current upstream does not fully model PS4 permissions:

- ELF segment permission logic exists but final loader code currently sets `PAGE_EXECUTE_READWRITE`.
- `sceKernelQueryMemoryProtection` is stubbed to report CPU RWX/GPU RW.
- BufferCache actively switches data pages between read-only and read-write.

M6 should document these upstream limitations rather than “fix” them opportunistically.

### 5.5 Executable memory requirements

Executable memory is required by:

- mapped guest modules
- Xbyak unresolved-symbol trampolines
- Xbyak HLE stubs
- Xbyak CodePatcher blocks

This is the bridge from M6 memory compatibility into the separate M7 execution gate.

### 5.6 M6 required tests

M6 diagnostics should, at minimum, validate without launching full emulation:

- ability to reserve the required large range or the precise subset upstream actually requires;
- alignment/granularity and host page size;
- commit inside reservation;
- decommit behavior;
- exact/fixed-address mapping;
- scan/query semantics;
- 16 KiB-aligned guest allocations;
- protection transitions RW <-> RO;
- executable protections **as capability data only** where safe/allowed;
- near-address allocation needed by CodePatcher (record for M7);
- file-backed mapping behavior if still required;
- write-fault tracking mechanism/capability;
- actual accessible address range on Xbox Dev Mode.

Unknown capabilities must remain UNKNOWN/REQUIRES_RUNTIME_TEST.

---

## 6. JIT/native execution architecture audit

### 6.1 Backend

There is no general JIT backend. Native x86-64 guest code is executed directly. Zydis and Xbyak support compatibility patching and generated bridging stubs.

### 6.2 Code cache / generated code

There is no single centralized code cache. Generated executable blocks are owned by:

- `App::unresolved_symbol_handlers`
- global `stubbed_symbol_handlers`
- `CodePatcher` allocations near patched guest code

This fragmentation should be accounted for in M7; it should not be refactored in M1.

### 6.3 ABI assumptions

- `PS4_FUNC` uses System V x86-64 ABI.
- inline assembly explicitly uses SysV argument registers (`rdi`, `rsi`) before guest entry.
- Xbyak stubs place parameters in SysV registers.
- host is currently Windows, making ABI bridging an intentional part of the emulator.

### 6.4 TLS assumptions

`OS::Thread::init()` accesses Windows TLS internals via `_tls_index` and `gs:[0x58]`, computes an offset to a `thread_local` guest TLS pointer, and CodePatcher rewrites guest TLS instructions to recover guest TLS from host TLS.

This is one of the highest M7 risks.

### 6.5 M7 risk list (no implementation)

- dynamic executable pages may be restricted by UWP/Xbox policy;
- code generated after launch may be restricted even if packaged executable code works;
- fixed/near executable mappings may fail;
- instruction-cache synchronization behavior must be determined;
- `_tls_index` / TEB-style assumptions may be unavailable or unsuitable;
- SysV ABI attributes and inline assembly must compile and behave correctly under the Xbox C++ toolchain;
- pthread-win32 representation may not be usable;
- red-zone patch strategy is Windows-specific and tied to exception/stack behavior.

M7 must not be started until M5/M6 provide facts.

---

## 7. Proposed Xbox architecture

```text
                    PS4 EMULATOR CORE
+----------------------------------------------------------+
| SELF/ELF + Linker | HLE/Sce APIs | GCN PM4/Shader logic |
+-------------------+--------------+-----------------------+
          |                |                 |
          +----------------+-----------------+
                           |
                           v
                 HOST PLATFORM CONTRACTS
+----------------------------------------------------------+
| lifecycle/logging | storage/files | time | input | audio |
| host threads      | virtual memory capability/primitives |
| graphics backend creation / presentation handoff         |
+----------------------------------------------------------+
              |                             |
              v                             v
       DESKTOP IMPLEMENTATION          XBOX/UWP IMPLEMENTATION
       SDL / Win32 where used          UWP-safe host services
              |                             |
              v                             v
          Vulkan backend                 D3D12 backend
```

### Layer recommendations

**Platform layer**  
Small host service contracts. It must not become a second emulator framework or mirror every C++ standard API.

**Graphics layer**  
Retain PM4/GCN frontend. Refine `Renderer` to remove SDL ownership and choose backend through a factory/context. Add D3D12 as sibling in M4.

**Memory layer**  
Centralize current host VM primitives only after M5 measures what Xbox supports. Preserve direct guest-address requirements until proven impossible.

**Filesystem layer**  
Keep PS4 mount/path/fd semantics; replace direct host file/storage mechanics below them.

**Input layer**  
Keep `ScePad` PS4 state/translation; feed host-neutral snapshots/events.

**Audio layer**  
Keep `SceAudioOut` PS4 ports/formats; send samples to host-neutral audio sink.

---

## 8. Xbox/UWP risk matrix

| Risk | Severity | Milestone owner | Current evidence |
|---|---|---|---|
| Huge process VA reservation | Critical | M5/M6 | hard-coded current Win32 path |
| Exact/fixed mappings | Critical | M5/M6 | kernel/loader requirement |
| Executable/dynamic pages | Critical | M5/M7 | ELF + Xbyak + patches |
| Native SysV guest execution on UWP host | Critical | M7 | direct jump + ABI attributes |
| Windows TLS internals | Critical | M7 | hard-coded `_tls_index`/GS access |
| Guest pthread host representation | High | M2/M5/M7 | pthread-win32 direct forwarding |
| Write-fault GPU dirty tracking | High | M5/M6/M4 | VirtualProtect + VEH |
| Vulkan-only construction | High but isolated | M2/M4 | `initVulkan()` hard-coded |
| Shader output tied to GLSL/SPIR-V | High | M4 | glslang compiler path |
| SDL renderer/window/input coupling | Medium | M2/M3/M4 | explicit TODO in `flip()` |
| Host storage model | Medium | M2/M3 | std::filesystem + CRT + SDL pref path |
| Networking restrictions | Medium/unknown | M5 | Asio/Winsock current backend |
| Audio API availability | Medium/unknown | M3/M5 | SDL current backend |
| cppco/toolchain behavior | Unknown | M4/M5 | async compute context switch path |

---

## 9. Recommended migration order

1. **M2 — isolate minimum platform contracts while preserving desktop behavior.** Focus on lifecycle/path/logging/timing/input/audio and carefully identified memory/thread API surfaces. Do not “solve” native execution.
2. **M3 — add UWP host that can launch diagnostics independently of PS4 execution.** This proves packaging/lifecycle/storage/input and D3D initialization handoff without touching guest runtime.
3. **M4 — D3D12 foundation as sibling Renderer backend.** First host primitives, then clear/triangle/quad, then prepare interfaces for GCN integration. Do not port Vulkan implementation line-for-line.
4. **M5 — capability probe on actual Xbox Dev Mode.** Memory/JIT/thread/graphics/UWP facts become authoritative.
5. **M6 — reproduce upstream guest VA model as far as measured Xbox capabilities permit.** Stop/document if fixed reservation/mapping cannot be achieved.
6. **M7 — separate decision on native execution/dynamic code/TLS/ABI.** Not part of the present task.

---

## 10. Files likely to change later

### M2/M3 platform boundary candidates

- `ChonkyStation4/ChonkyStation4.cpp`
- `ChonkyStation4/PlayStation4.cpp/.hpp`
- `ChonkyStation4/Common/Logger.hpp`
- `ChonkyStation4/OS/Filesystem.cpp/.hpp`
- `ChonkyStation4/OS/UserManagement.cpp/.hpp`
- `ChonkyStation4/OS/Libraries/ScePad/*`
- `ChonkyStation4/OS/Libraries/SceAudioOut/*`
- selected timing/thread host calls
- CMake/build target definitions
- new platform/host files

### M4 graphics candidates

- `GCN/Backends/Renderer.hpp`
- `GCN/GCN.hpp/.cpp`
- small Vulkan host-responsibility cleanup
- new D3D12 backend files
- shader backend seam
- new D3D12 resource/cache/pipeline files

### M5/M6 memory/capability candidates

- new capability probe project/component
- new host VM primitives/diagnostics
- only the minimal loader/kernel/cache call sites required to consume the VM boundary

### M7 candidates — explicitly later

- `Loaders/ELF/CodePatcher.*`
- `Loaders/Linker/Linker.cpp`
- `Loaders/Module.hpp`
- `Loaders/App.cpp`
- `OS/Thread.*`
- guest pthread implementation as required by chosen execution strategy

---

## 11. Files that should remain untouched during early Xbox platform work

Unless a concrete interface deficiency is proven, do not add Xbox branches to:

- `GCN/PM4.hpp`
- `GCN/RegisterOffsets.hpp`
- `GCN/Detiler/**`
- `GCN/Shader/Decoder.*`
- `GCN/Shader/Instruction.*`
- most `GCN/CommandProcessor.*`
- PS4 register/packet/format definitions
- SFO parser
- NID/symbol identity logic
- guest API constants/structures unrelated to host transport
- unrelated HLE functionality

SELF/ELF parsing should also remain unchanged except at the host file/memory-placement boundary when M6 requires it.

---

## 12. Validation ledger for M1

M1 changes are documentation-only. The source archive inherited the M0 baseline-build limitation: GitHub's source ZIP has empty Git submodule directories, so CMake configuration cannot complete without materializing the pinned dependencies; Vulkan development packages are also absent in this execution environment.

M1 validation must therefore distinguish repository/source integrity from unavailable dependency material. No CMake logic will be modified merely to bypass those missing upstream dependencies.

The final command/result details are recorded after validation at the bottom of this file.

---

## 13. M1 conclusion

The project has a viable Xbox insertion architecture, but it is **not yet a platform-independent emulator**. The most important boundaries are:

1. host lifecycle/storage/input/audio services currently supplied by SDL/desktop APIs;
2. `Renderer` / GCN backend selection for D3D12;
3. a host virtual-memory capability/primitives layer for M5/M6;
4. native-execution TLS/ABI/dynamic-code behavior reserved for M7.

The critical architecture rule for subsequent work is:

```text
Do not turn Xbox support into #ifdefs across PS4 emulation logic.
Move host responsibilities to narrow seams, keep the GCN/loader/HLE semantics authoritative,
and let measured Xbox capabilities decide M6/M7 behavior.
```

---

## 14. Final M1 validation results

### Build/configure command

```bash
cmake -S /mnt/data/chonkystation4-xbox-port \
      -B /mnt/data/chonkystation4-m1/build \
      -G Ninja \
      -DCMAKE_BUILD_TYPE=Release
```

**Result:** `BLOCKED` during configure, exit code `1`.

The failure is identical in class to the already documented M0 source-archive environment blocker and is not caused by M1. The supplied GitHub source archive contains empty submodule directories, so CMake cannot find the nested CMake projects for Zydis, SDL, xxHash, miniz, cpp-httplib, glslang or cppco. The environment also lacks Vulkan headers/libraries required by the untouched upstream `find_package(Vulkan REQUIRED)` call.

No build-system or emulator source workaround was added.

### Build command

```bash
cmake --build /mnt/data/chonkystation4-m1/build --config Release
```

**Result:** `BLOCKED`, exit code `1`, because configure did not generate `build.ninja`.

### Test command

```bash
ctest --test-dir /mnt/data/chonkystation4-m1/build --output-on-failure
```

**Result:** `NOT TESTED / NO TESTS FOUND`. CTest exited `0` and reported `No tests were found!!!`.

### Repository validation

Only the following repository files were created by M1:

- `docs/XBOX_ARCHITECTURE.md`
- `docs/M1_ARCHITECTURE_AUDIT.md`

No emulator implementation, CMake, dependency, Xbox, UWP or D3D12 source files were modified.

### Milestone status

**M1: PASS**

Rationale: every requested M1 analysis/documentation objective was completed against the verified upstream source, the architecture boundaries and risks are based on actual call sites, implementation code was left untouched, and the attempted build/test validation reproduces only the pre-existing M0 dependency-material blocker. M2 has not been started.
