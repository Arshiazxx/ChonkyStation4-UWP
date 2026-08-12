# ChonkyStation4 Xbox/UWP Architecture

**Milestone:** M1 — Architecture Audit  
**Authoritative source:** `liuk7071/ChonkyStation4`  
**Upstream commit:** `310269290a3c256f5911d4bc7e441489bffffbf6`  
**Scope:** Architecture only. No Xbox implementation, UWP project, D3D12 backend, memory rewrite, or JIT change is introduced by this document.

## 1. Architectural objective

The Xbox port should preserve the current emulator architecture and insert host/platform boundaries where the upstream implementation currently reaches directly into SDL, Win32, POSIX/pthread, Vulkan, filesystem, networking, and audio APIs.

The intended direction is:

```text
Current ChonkyStation4 emulation systems
                |
                v
      narrow platform contracts
                |
       +--------+---------+
       |                  |
       v                  v
Desktop host         Xbox/UWP host
(existing path)      (future path)
       |                  |
       v                  v
Vulkan backend       D3D12 backend
(existing)           (future)
```

The port should **not** place Xbox conditionals throughout the PS4 HLE, loader, command processor, shader decoder, or guest-facing APIs. The high-value boundary is at host services and graphics/memory backends.

## 2. Current upstream subsystem map

```text
ChonkyStation4.cpp
  Desktop CLI / process bootstrap
  |- early 2 TiB Win32 address reservation
  |- SDL preference path / user setup
  `- PS4::loadAndRun()
        |
        v
PlayStation4.cpp
  Emulator orchestration
  |- OS::Thread::init()
  |- game/AppLoader OR ELF Linker
  |- HLE module construction + relocations
  `- App::run()
        |
        v
Loaders/App.cpp
  Guest main thread
  |- PS4::init()
  |    |- starts GCN thread
  |    |- mounts host-backed guest filesystem
  |    `- waits for renderer init
  |- LLE module initializers
  `- direct x86-64 jump to guest entry point

Loader / runtime path
  Loaders/App/AppLoader.*
  Loaders/ELF/ELFLoader.*
  Loaders/ELF/CodePatcher.*
  Loaders/Linker/Linker.*
  Loaders/Module.hpp
    |- SELF/ELF parse and direct host-address mapping
    |- relocations
    |- TLS metadata
    |- Xbyak runtime stubs/trampolines
    `- Zydis/Xbyak guest-code patching

HLE / kernel path
  OS/HLE.*
  OS/Libraries/Kernel/*
  OS/Libraries/Sce*/*
    |- symbol-export based HLE dispatch
    |- guest objects/events/semaphores
    |- pthread forwarding
    |- guest filesystem calls
    |- networking/audio/input/video APIs
    `- memory mapping APIs

GPU path
  SceGnmDriver / SceVideoOut
        |
        v
  GCN command queues
        |
        v
  GCN/CommandProcessor.*
        |
        v
  GCN/Backends/Renderer.hpp
        |
        v
  VulkanRenderer + Vulkan caches/pipelines
        |
        v
  SDL window + Vulkan presentation

Shader path
  GCN ISA / Decoder / ShaderDecompiler
        |
        v
  ShaderData (currently GLSL source-centric)
        |
        v
  Vulkan ShaderCache
        |
        v
  glslang -> SPIR-V -> VkShaderModule
```

## 3. Important architectural reality: CPU execution is native x86-64

The current project does not contain a conventional CPU interpreter or a portable guest-to-host JIT backend. PS4 guest code is already x86-64 and is mapped into the host address space, relocated, selectively patched, and then executed directly.

Relevant files:

- `ChonkyStation4/Loaders/App.cpp`
- `ChonkyStation4/Loaders/ELF/ELFLoader.cpp`
- `ChonkyStation4/Loaders/ELF/CodePatcher.cpp`
- `ChonkyStation4/Loaders/Linker/Linker.cpp`
- `ChonkyStation4/Loaders/Module.hpp`
- `ChonkyStation4/OS/Thread.cpp`

`initAndJumpToEntry()` uses x86-64 inline assembly and jumps directly to the guest entry address. `PS4_FUNC` is defined as `__attribute__((sysv_abi))`, while the current Windows host also contains Windows-specific TLS and stack handling. Xbyak creates executable trampolines/stubs, and `CodePatcher` creates executable code near guest instructions.

Therefore the future M7 gate is better described as **native guest execution / dynamic-code compatibility**, not simply “port a JIT backend.” M2 must not attempt to solve it.

## 4. Proposed platform boundaries

### 4.1 Host application/lifecycle boundary

**Current coupling**

- `ChonkyStation4.cpp` is a desktop `main()` with CLI11.
- SDL provides preference paths.
- VulkanRenderer creates the SDL window and handles process/window events.
- renderer flip currently polls input and manages fullscreen/title/FPS.

**Future boundary**

A host-facing bootstrap contract should own:

- application lifecycle
- app-data/storage roots
- logging destination
- window/presentation surface ownership
- suspend/resume/shutdown notifications
- host input polling/events
- host audio device lifecycle
- graphics-backend selection/creation

The existing desktop executable can remain one host. A future UWP executable should become another host without forcing UWP lifecycle code into guest/HLE systems.

### 4.2 Filesystem boundary

The current `PS4::FS` layer is a useful **guest filesystem namespace** but is not a complete host abstraction. `FS::File` stores a `FILE*`; implementation uses `std::filesystem`, CRT file operations, and Kernel file-backed `mmap` reaches through the FS object to `_fileno/_get_osfhandle/CreateFileMappingW`.

Keep guest-facing device semantics (`APP0`, `SAVEDATA0`, `DEV`, `TEMP0`, `SYSTEM`, `SYSTEM_EX`) stable. Introduce host storage/file primitives beneath them later rather than replacing PS4 filesystem semantics.

Recommended separation:

```text
PS4 guest paths / fd semantics
          |
          v
      PS4::FS
          |
          v
Host filesystem/storage contract
     /                 \
Desktop CRT/fs       UWP storage implementation
```

### 4.3 Timing boundary

Most scheduling/timing uses portable `std::chrono`, which should stay. SDL performance counter/tick use appears in kernel timing, pad timestamps, and renderer FPS/event logic. These should eventually depend on a small monotonic-clock service rather than SDL.

### 4.4 Threading boundary

There are two different threading concerns:

1. **Emulator internal concurrency** — much of GCN/event synchronization uses `std::thread`, `std::mutex`, `std::condition_variable`, `std::counting_semaphore`, atomics and chrono. This is largely portable.
2. **Guest pthread emulation and native-execution TLS** — `OS/Thread.*` and `OS/Libraries/Kernel/pthread/*` rely directly on pthread-win32 plus Win32 TLS/stack/thread APIs. This is a deeper compatibility boundary.

M2 should isolate generic host thread operations where possible, but must not pretend that replacing `std::thread` or thread naming solves guest pthread/TLS semantics. Those semantics are tied to M5/M7 capability results.

### 4.5 Input boundary

`ScePad` is guest-facing PS4 controller emulation but directly polls SDL keyboard/controller state and stores `SDL_GameController*`. VulkanRenderer also owns SDL controller hotplug handling.

Target architecture:

```text
ScePad guest API/state translation
             |
             v
     Host input snapshot/events
       /                 \
SDL desktop input       Xbox/UWP input
```

Preserve PS4 button/stick semantics in `ScePad`; remove renderer ownership of generic input in a later milestone.

### 4.6 Audio boundary

`SceAudioOut` currently implements PS4 port semantics and SDL playback in the same file. The PS4-facing port objects/format logic can remain, while queued PCM playback should target a host audio sink contract.

```text
SceAudioOut guest API
       |
format/port translation
       |
Host audio sink
   /          \
SDL        Xbox/UWP
```

### 4.7 Networking boundary

`SceNet` uses standalone Asio directly, while the build links `Ws2_32` on Windows. Other PSN/web code uses Asio and cpp-httplib.

This area should not be rewritten in M2 unless needed to compile the host. Introduce a network capability/service boundary only where UWP API/capability differences require it. Network availability and permissions belong in the M5 capability matrix.

### 4.8 Memory boundary

Memory is not an ordinary service abstraction; it is part of the emulator execution model.

Current dependencies include:

- 2 TiB reservation starting at `0x0000000080000000`
- `VirtualQuery` scanning reserved address space
- fixed/near-address commitments
- `VirtualAlloc` reserve/commit behavior
- `VirtualProtect` including executable permissions
- `VirtualFree(..., MEM_DECOMMIT)`
- file-backed `CreateFileMappingW` / `MapViewOfFile`
- vectored exception handling for GPU buffer dirty tracking
- host page-size discovery

This must become a **host virtual-memory contract**, but M2 should define boundaries only to the extent justified by current call sites. M5 must measure Xbox behavior; M6 must reproduce the upstream guest-memory model; M7 owns executable/dynamic-code consequences.

Suggested conceptual contract areas, not an implementation specification:

```text
Host virtual memory
|- reserve range
|- inspect/query range
|- commit/decommit
|- protect pages
|- fixed/near placement
|- file-backed mapping (if required)
`- write-fault tracking mechanism/capability
```

Do not hide unsupported behavior behind fake successful return values.

## 5. Graphics architecture and D3D12 insertion point

### 5.1 What is already reusable

The strongest existing GPU backend boundary is:

- `GCN/CommandProcessor.*` interprets PM4 packets and manipulates emulated GCN state.
- `GCN/Backends/Renderer.hpp` exposes backend operations:
  - `init`
  - `draw`
  - `drawIndirect`
  - `dispatch`
  - `flip`
  - `fillGDS`
- register/state helper logic is shared through `Renderer`.

The command processor calls the abstract `Renderer`; therefore it should remain the primary boundary between PS4 GPU command emulation and a future D3D12 implementation.

### 5.2 What is not abstracted yet

`Renderer` is only partially backend-neutral:

- it includes SDL and owns `SDL_Window*`;
- `GCN.hpp` directly includes `VulkanRenderer.hpp` and constructs it in `initVulkan()`;
- VulkanRenderer owns SDL initialization/window/events/input/FPS behavior;
- shader cache expects GLSL -> glslang -> SPIR-V;
- resource caches/pipeline/render-target classes are Vulkan types;
- Vulkan BufferCache performs Win32 page protection and vectored exception handling.

These are the minimal seams that later milestones should refine.

### 5.3 Future D3D12 path

```text
SceGnmDriver / SceVideoOut
           |
           v
GCN command queues + PM4 CommandProcessor
           |
           v
Backend-neutral Renderer contract
       /                    \
      v                      v
VulkanRenderer          D3D12Renderer
(desktop)               (Xbox/UWP)
      |                      |
Vulkan resources         D3D12 resources
SPIR-V pipeline          DXIL/HLSL-compatible pipeline
SDL/Vk swapchain         UWP/DXGI presentation
```

D3D12 should be introduced as a sibling backend rather than translating Vulkan calls or replacing the GCN command processor.

### 5.4 Shader integration boundary

The current shader path is not backend-neutral at its output:

- GCN decode/decompile lives under `GCN/Shader`.
- `ShaderData` contains a generated source string plus resource metadata.
- Vulkan ShaderCache calls `decompileShader()` then `compileGLSL()`.
- `compileGLSL()` explicitly targets Vulkan 1.3 and SPIR-V 1.2 through glslang.

The reusable part is the GCN decode/decompile semantics and descriptor/resource metadata. The Vulkan GLSL/SPIR-V compiler path is backend-specific. M4 will need a D3D12-compatible shader compilation/translation route at this seam; this document does not select or implement one.

## 6. Guest memory model relevant to M6

### 6.1 Process-wide reservation

On Windows desktop, `main()` attempts:

```text
base: 0x0000000080000000
size: 2048 GiB (2 TiB)
operation: MEM_RESERVE, PAGE_NOACCESS
```

Kernel allocators generally scan/commit within a 2000 GiB subrange beginning at `0x80000000`. A second system mapping search base is `0x001000000000` (64 GiB).

### 6.2 ELF/SELF loading

`ELFLoader`:

- parses SELF container data and embedded ELF metadata;
- starts module placement around `0x0000008000000000` (512 GiB);
- scans reserved host ranges using `VirtualQuery`;
- commits module memory directly into the host VA space;
- copies PT_LOAD segments to `base + guest virtual address`;
- records PT_TLS data;
- currently applies `PAGE_EXECUTE_READWRITE` to loaded segments after loading rather than faithfully enforcing each ELF segment permission.

### 6.3 Guest kernel mappings

Kernel memory APIs currently assume:

- guest page-size reporting of 16 KiB;
- 16 KiB default alignment for several mappings;
- ability to find/commit exact or near requested host virtual addresses;
- reserve-versus-commit distinction;
- decommit without necessarily releasing the whole process reservation;
- direct/flexible memory bookkeeping;
- file-backed read mappings;
- virtual-memory querying.

Some current upstream semantics are incomplete/stubbed (for example `sceKernelQueryMemoryProtection` and portions of reserve tracking). M6 should reproduce **what upstream actually requires**, not invent a more sophisticated memory manager before capability data exists.

### 6.4 GPU memory tracking coupling

The Vulkan buffer cache makes guest RAM read-only with `VirtualProtect`, catches writes through a Win32 vectored exception handler, marks cached GPU data dirty, and restores write permission. A future D3D12 backend will still need a coherent strategy for detecting guest writes, even if the Vulkan resource objects disappear.

This makes dirty-memory tracking an integration point between **host VM services** and **GPU resource caching**, not purely a Vulkan concern.

## 7. Native execution/JIT risks for M7

The following are intentionally **not solved in M1–M6** unless a prerequisite capability probe needs to measure them:

- executable committed pages for loaded guest code;
- Xbyak executable allocations for HLE/unresolved-symbol trampolines;
- executable near-code allocations used by `CodePatcher`;
- instruction-cache coherency requirements after dynamic code writes;
- direct native x86-64 jump into guest entry;
- System V ABI guest call convention on a Windows/UWP host;
- Windows-specific host TLS lookup using `_tls_index` and `gs:[0x58]`;
- patching guest `fs:` TLS accesses to host TLS storage;
- red-zone patching designed around Windows exception/stack behavior;
- pthread-win32 guest-thread representation and stack metadata.

These make executable-memory/TLS/ABI capability results from M5 a hard input to M7.

## 8. Recommended Xbox layer structure

This is a responsibility map, not a requirement to create these exact directories in M2.

```text
Emulator systems (retain current ownership)
|- Loaders / ELF / Linker
|- OS HLE and Sce libraries
|- GCN command processor + shader decoder
`- guest-facing FS/input/audio semantics

Platform contracts (narrow, host-oriented)
|- application/lifecycle + logging
|- paths/storage/files
|- monotonic timing
|- thread utilities / capability surface
|- input source
|- audio sink
|- virtual memory primitives
`- graphics backend factory/context handoff

Desktop implementation
|- existing SDL services where appropriate
|- current Win32 VM implementation
`- Vulkan backend

Xbox/UWP implementation (future)
|- UWP lifecycle/storage/input/audio
|- Xbox-tested VM implementation
`- D3D12 backend + UWP/DXGI presentation
```

## 9. Files expected to change in later milestones

### Likely boundary/refactor candidates

- `ChonkyStation4/ChonkyStation4.cpp`
- `ChonkyStation4/PlayStation4.cpp/.hpp`
- `ChonkyStation4/Common/Logger.hpp`
- `ChonkyStation4/OS/Filesystem.cpp/.hpp`
- `ChonkyStation4/OS/UserManagement.cpp/.hpp`
- `ChonkyStation4/OS/Thread.cpp/.hpp`
- `ChonkyStation4/OS/Libraries/Kernel/Kernel.cpp`
- `ChonkyStation4/OS/Libraries/Kernel/pthread/*`
- `ChonkyStation4/OS/Libraries/ScePad/*`
- `ChonkyStation4/OS/Libraries/SceAudioOut/*`
- `ChonkyStation4/GCN/GCN.cpp/.hpp`
- `ChonkyStation4/GCN/Backends/Renderer.hpp`
- `ChonkyStation4/GCN/Backends/Vulkan/VulkanRenderer.cpp` (only to remove generic host responsibilities if needed)
- build-system files to introduce independent desktop/Xbox targets

### New later components expected conceptually

- host/platform interfaces
- desktop adapters preserving current behavior
- Xbox/UWP host/adapters
- D3D12 Renderer implementation and D3D12 resource/pipeline/shader classes
- capability probe
- Xbox VM diagnostic/memory implementation

## 10. Files that should remain untouched unless an actual backend contract proves insufficient

The following are not candidates for Xbox-specific edits during the platform bring-up:

- `GCN/PM4.hpp`
- `GCN/RegisterOffsets.hpp`
- `GCN/Detiler/**`
- `GCN/Shader/Decoder.*`
- `GCN/Shader/Instruction.*`
- GCN ISA/register definitions generally
- SELF/ELF parsing logic that is not host-memory placement
- SFO parsing
- symbol/NID semantics
- guest Sce API structures/constants unrelated to host services
- game-specific emulator logic unrelated to platform integration

`GCN/CommandProcessor.*` should also remain stable if the existing `Renderer` operations are sufficient. If a D3D12 backend later exposes a real missing abstraction, change the interface deliberately rather than adding Xbox branches inside command decoding.

## 11. Milestone boundaries

### M2 — platform layer

Focus on clean host-service boundaries and preserving desktop behavior. Do not solve PS4 GPU translation, guest executable memory, or full UWP hosting here.

### M3 — UWP host

Own lifecycle, diagnostic UI, storage roots, controller hookup, logging, shutdown, and graphics initialization handoff. It should be capable of launching without starting PS4 execution.

### M4 — D3D12 backend

Implement a sibling renderer foundation through the GPU backend boundary. Keep PM4/GCN command emulation upstream-owned.

### M5 — capability probe

Measure the Xbox runtime facts that cannot safely be inferred from desktop/UWP documentation, especially virtual memory, executable memory, threading, graphics, storage/network/audio/input behavior.

### M6 — guest memory

Implement/validate the minimum host VM environment required by the current upstream mapping model. Do not redesign native execution.

### M7 — separate gate

Decide native guest execution/dynamic-code strategy using M5/M6 results. This milestone is explicitly out of scope here.

## 12. Primary risks

| Risk | Layer | Why it matters |
|---|---|---|
| Large fixed virtual-address reservation | Memory | Fundamental to current direct guest-address mapping |
| Fixed/near-address commitment | Memory / runtime patching | Loader and CodePatcher depend on placement |
| Executable memory/dynamic code | M7 | Guest code and Xbyak trampolines require execution |
| Windows TLS assumptions | M7 | Current guest TLS patching reads Windows TLS internals |
| pthread-win32 dependence | Threading | Guest pthread forwarding is host-implementation-coupled |
| Win32 vectored write-fault tracking | Memory/GPU | Vulkan BufferCache coherence currently depends on it |
| Vulkan-only backend construction | Graphics | Current startup cannot select D3D12 without a factory seam |
| GLSL/SPIR-V-only shader output | Graphics/shaders | D3D12 needs an independent compiler/translation path |
| SDL responsibilities inside renderer | Host/graphics | Window, input, lifecycle and timing are mixed with Vulkan |
| CRT/std::filesystem backing model | Filesystem | UWP storage rules differ; guest path semantics should remain |
| UWP network/audio API/capability differences | Services | Must be verified rather than assumed |

## 13. Architecture decision summary

The correct Xbox port is **not** “convert the emulator from Vulkan/SDL to UWP everywhere.” It is:

```text
Current upstream PS4 emulation logic
              |
              v
Isolate host service boundaries
              |
       +------+------+
       |             |
       v             v
Desktop          Xbox/UWP
       |             |
       v             v
Vulkan          D3D12
```

The guest CPU/memory model is a separate technical gate from graphics and frontend work. Keeping those concerns separated is essential to reach M7 with measured capability data rather than accumulated platform hacks.
