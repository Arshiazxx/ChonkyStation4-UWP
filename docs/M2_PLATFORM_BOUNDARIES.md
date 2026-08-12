# M2 — Platform Boundary Extraction

Upstream source of truth: `liuk7071/ChonkyStation4` at `310269290a3c256f5911d4bc7e441489bffffbf6`.

## Status

**PASS** for the architectural extraction implemented in M2.

The full upstream executable cannot be compiled in this environment because the supplied GitHub source archive does not contain the contents of the repository's Git submodules and this Linux container has no Vulkan SDK. The exact blocker is unchanged from M0/M1 and occurs during CMake configuration before any M2 source is compiled. M2 therefore does not claim a successful native executable build.

## Goal

M2 introduces only the minimum seams needed so future host and renderer backends do not have to modify GCN command processing or duplicate emulator logic.

No Xbox implementation, UWP application, D3D12 implementation, guest-memory rewrite, or JIT change is part of this milestone.

## Architecture changes

### 1. Renderer construction seam

Before M2:

```text
GCN::gcnThread
    -> GCN::initVulkan()
        -> std::make_unique<VulkanRenderer>()
```

After M2:

```text
GCN::gcnThread
    -> GCN::initRenderer()
        -> createRenderer(RendererBackend::Vulkan)
            -> VulkanRenderer
```

Files:

- `ChonkyStation4/GCN/Backends/RendererFactory.hpp`
- `ChonkyStation4/GCN/Backends/RendererFactory.cpp`
- `ChonkyStation4/GCN/GCN.hpp`
- `ChonkyStation4/GCN/GCN.cpp`

The default remains Vulkan. No configuration behavior changes in M2.

Future M4 integration is intentionally narrow: add `RendererBackend::D3D12` and construct a D3D12 implementation in the factory. GCN PM4 submission and command decoding do not need to know which host API is selected.

### 2. SDL removed from the generic renderer interface

Before M2, `Renderer.hpp` included SDL and exposed an `SDL_Window*` field. That made every future renderer backend inherit a desktop SDL window concept even though it is not part of PS4 GPU emulation.

M2 moves the window field and SDL include into `VulkanRenderer` itself.

```text
Renderer
  - GCN register state
  - draw/dispatch/flip/GDS interface

VulkanRenderer
  - SDL_Window
  - Vulkan surface/swapchain
  - current desktop event/input/window behavior
```

This does not alter Vulkan behavior: `VulkanRenderer` still creates and uses the same SDL window.

### 3. Safe polymorphic destruction

`Renderer` already existed as a polymorphic base stored in `std::unique_ptr<Renderer>`, but it did not declare a virtual destructor. M2 adds `virtual ~Renderer() = default;` so backend objects can be safely destroyed through the interface.

## VulkanRenderer responsibility audit

The following responsibilities remain inside `VulkanRenderer` after M2 deliberately, because moving them would be a larger behavioral change than required for the first boundary extraction:

| Responsibility | Current location | M2 action | Later destination |
|---|---|---|---|
| Vulkan device/surface/swapchain | `VulkanRenderer` | Keep | Vulkan backend |
| SDL window creation | `VulkanRenderer::init` | Keep but hide from base interface | desktop host/window service |
| SDL event processing | `VulkanRenderer::flip` | Keep | host lifecycle/event pump |
| controller hotplug/polling | `VulkanRenderer::flip` | Keep | input platform service |
| fullscreen toggle | `VulkanRenderer::flip` | Keep | desktop window service |
| FPS timing | `VulkanRenderer::flip` | Keep | host/presentation telemetry |
| title updates | `VulkanRenderer::flip` | Keep | desktop host/window service |
| PS4 render state and draw/dispatch | renderer backend | Keep | renderer backend |

Moving these responsibilities is justified later when an actual second host/backend exists. Creating abstract interfaces for them in M2 without a consumer would violate the requirement not to add unused abstraction classes.

## Host contracts considered but intentionally not added yet

### Logging

Current logging is usable by both emulator and platform code and does not presently force SDL/Vulkan ownership. No interface was introduced.

### Filesystem/storage

The emulator's guest filesystem mapping is a larger semantic subsystem. M3 can provide UWP storage primitives for the host without replacing `PS4::FS`. M6 will separately address guest virtual-memory requirements. No unused filesystem abstraction was introduced in M2.

### Timing

Core code already uses standard C++ clocks in several places. SDL-specific presentation timing remains isolated in Vulkan code. No timing interface was added yet.

### Input

Current controller polling is coupled to SDL in `VulkanRenderer::flip`. The dependency is documented but not moved until the Xbox host has an actual input implementation to consume an interface.

### Lifecycle

Desktop lifetime currently derives from process lifetime/SDL events. UWP lifecycle belongs in M3 and should not be modeled speculatively in core code during M2.

## Future Xbox integration points

```text
PS4 GCN command processing
        |
        v
Renderer interface
        |
        v
RendererFactory
   |             |
   v             v
Vulkan       D3D12 (M4)
Desktop      Xbox/UWP host surface
```

Platform services should remain orthogonal to renderer selection:

```text
Host application
  |-- lifecycle
  |-- storage/filesystem access
  |-- input
  |-- audio device
  |-- timing/logging
  `-- graphics host/surface
```

The emulator core should consume these only where host behavior is actually required.

## Systems intentionally untouched

M2 does not change:

- GCN PM4 decoding or command processing;
- shader decoding/decompilation;
- Vulkan pipeline/resource/cache implementation;
- loader/SELF/ELF handling;
- guest filesystem semantics;
- SceKernel/HLE behavior;
- guest pthread emulation;
- guest address-space layout;
- executable-memory/code patching behavior;
- TLS/ABI/native guest execution;
- audio emulation;
- networking emulation.

## Files changed

- `CMakeLists.txt` — adds the renderer factory sources only.
- `ChonkyStation4/GCN/Backends/Renderer.hpp` — removes SDL ownership from the generic interface and adds a virtual destructor.
- `ChonkyStation4/GCN/Backends/RendererFactory.hpp` — new renderer-selection API.
- `ChonkyStation4/GCN/Backends/RendererFactory.cpp` — default Vulkan construction.
- `ChonkyStation4/GCN/Backends/Vulkan/VulkanRenderer.hpp` — owns the SDL window previously exposed by `Renderer`.
- `ChonkyStation4/GCN/GCN.hpp` — uses the renderer factory instead of directly naming Vulkan.
- `ChonkyStation4/GCN/GCN.cpp` — calls `initRenderer()`.
- `docs/M2_PLATFORM_BOUNDARIES.md` — this record.

## Validation

### Configure

```bash
cmake -S /mnt/data/chonkystation4-xbox-port \
      -B /mnt/data/chonkystation4-m2/build \
      -G Ninja \
      -DCMAKE_BUILD_TYPE=Release
```

Result: **BLOCKED by pre-existing dependency environment**.

CMake stops because the source archive has empty submodule directories for Zydis, SDL, xxHash, miniz, cpp-httplib, glslang and cppco, and because the container has no Vulkan development package.

Log: `/mnt/data/chonkystation4-m2/M2_CONFIGURE.log`.

### Build

```bash
cmake --build /mnt/data/chonkystation4-m2/build
```

Result: **BLOCKED** because configuration did not produce `build.ninja`.

Log: `/mnt/data/chonkystation4-m2/M2_BUILD.log`.

### Tests

```bash
ctest --test-dir /mnt/data/chonkystation4-m2/build --output-on-failure
```

Result: **NOT TESTED — no tests found**.

### Static validation

- `git diff --check`: PASS.
- Generic `Renderer.hpp` no longer includes SDL: PASS.
- `GCN.hpp` no longer includes `VulkanRenderer.hpp`: PASS.
- default renderer factory path remains Vulkan: PASS by source inspection.

## Known issues

1. Full compilation remains unavailable until the upstream submodules and Vulkan SDK are materialized.
2. Vulkan's event/input/window responsibilities are still coupled inside `flip()`; this is deliberate in M2.
3. Renderer backend selection is not yet user-configurable; only Vulkan exists at this milestone.

## Milestone ledger

- Milestone status: **PASS**
- Implementation validation: static/source validation PASS
- Full build: **BLOCKED** by existing dependency environment
- Tests: **NOT TESTED** (no upstream tests found)
- Xbox runtime: **NOT TESTED**
