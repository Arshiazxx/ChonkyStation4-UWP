# M4 — D3D12 Backend Foundation

Authoritative upstream: `liuk7071/ChonkyStation4` at `310269290a3c256f5911d4bc7e441489bffffbf6`.

## Status

**PARTIAL**.

The D3D12 host foundation is implemented and statically validated. A Windows/UWP build and runtime render test could not be executed in this Linux environment because MSBuild/MSVC/Windows SDK/AppX tooling is absent. No Xbox hardware success is claimed.

## Architecture

```text
M3 UWP host
    |
    v
SwapChainPanel
    |
    v
Graphics::D3D12Backend
    |-- hardware adapter selection
    |-- ID3D12Device
    |-- direct command queue
    |-- command allocator/list
    |-- composition swap chain
    |-- RTV/SRV descriptor heaps
    |-- upload/default resources
    |-- root signature + PSOs
    |-- shader compilation
    `-- fence/event synchronization
```

Future emulator integration remains:

```text
PS4 GCN/PM4
    |
    v
Renderer interface (M2)
    |
    v
future D3D12Renderer adapter
    |
    v
reusable M4 D3D12 mechanics
```

M4 does **not** register a D3D12 implementation in `RendererFactory`, because the current PS4 `Renderer` contract includes PS4 draw/dispatch/flip semantics that are not implemented yet. Registering a stub backend would fake emulator functionality.

## Implemented foundation

### Device and adapter

- creates a DXGI factory;
- enumerates hardware adapters;
- skips software adapters;
- requires a hardware adapter that accepts D3D12 feature level 11_0 or higher;
- queries maximum supported feature level;
- queries shader model support for diagnostics.

There is intentionally no WARP/software fallback.

### Command submission

- direct command queue;
- direct command allocator;
- graphics command list;
- execute/close/reset lifecycle.

### Presentation

- UWP/XAML `SwapChainPanel` interop through `ISwapChainPanelNative`;
- `CreateSwapChainForComposition`;
- two BGRA8 buffers;
- `DXGI_SCALING_STRETCH`;
- flip-sequential presentation;
- RTV creation for each back buffer;
- explicit PRESENT ↔ RENDER_TARGET transitions.

The swap chain is detached from the XAML panel on shutdown.

### Descriptor management

- RTV descriptor heap for swap-chain back buffers;
- shader-visible CBV/SRV/UAV heap for the M4 test texture.

This is a foundation only. It is not yet a PS4 descriptor/resource cache.

### Resource management

- committed upload buffers for test geometry;
- default-heap 2x2 RGBA texture;
- upload buffer with aligned row pitch;
- copy and resource transition;
- SRV creation.

No VMA-equivalent allocator or emulator resource cache exists yet.

### Shader pipeline

M4 provides a diagnostic shader path using `D3DCompile` with simple HLSL vertex/pixel shaders targeting shader model 5.1 bytecode for D3D12 consumption.

Pipelines:

- colored triangle;
- textured quad.

A root signature contains one pixel-visible SRV descriptor table and one static point sampler.

This is **not** yet the PS4 shader translation path. Current upstream shader decompilation still targets the Vulkan/glslang/SPIR-V route.

### Synchronization

- ID3D12Fence;
- event-backed fence wait;
- queue signal;
- deterministic wait after uploads and each diagnostic frame.

Per-frame full GPU waits are intentionally simple and not optimized.

## Diagnostic render modes

The M3 host now exposes three UI buttons:

1. **Clear** — clears the swap-chain back buffer.
2. **Triangle** — draws a three-vertex colored triangle.
3. **Textured quad** — draws a six-vertex quad sampling a 2x2 test texture.

These are infrastructure tests only. They do not consume PS4 GPU commands.

## Xbox/UWP considerations

- Uses a XAML `SwapChainPanel` rather than SDL or HWND ownership.
- Uses a composition swap chain, keeping host UI separate from emulator graphics.
- Uses only a hardware D3D12 adapter; failure is reported instead of silently selecting software rendering.
- Device feature level and shader model remain runtime facts and must be measured on actual Xbox Dev Mode hardware.
- Runtime shader compilation availability on the target must also be validated on Windows/Xbox before it is relied on for emulator shaders.

## Future PS4 GPU integration

The next graphics integration work after capability validation should:

1. separate reusable D3D12 device/resource/sync services from the M4 diagnostic wrapper;
2. implement a real `PS4::GCN::Renderer`-compatible D3D12 backend;
3. map existing GCN render-target, buffer, texture, pipeline and synchronization semantics onto D3D12;
4. add a D3D12-compatible output from the shader translation pipeline;
5. only then add `RendererBackend::D3D12` to `RendererFactory`.

GCN command decoding should remain unchanged unless a concrete backend-neutral contract gap is discovered.

## Limitations

- no Windows/UWP compile validation in this environment;
- no Xbox launch validation;
- no device-lost recovery;
- no resize/orientation handling after initial swap-chain creation;
- one queue and one allocator;
- full GPU wait after each frame;
- no PS4 resource cache;
- no PS4 shader translation;
- no compute diagnostic pipeline;
- no integration with `SceVideoOut` or GCN command processing.

## Files changed

- `platform/xbox/ChonkyStation4.Xbox/Graphics/D3D12Backend.hpp`
- `platform/xbox/ChonkyStation4.Xbox/Graphics/D3D12Backend.cpp`
- `platform/xbox/ChonkyStation4.Xbox/MainPage.xaml`
- `platform/xbox/ChonkyStation4.Xbox/MainPage.xaml.h`
- `platform/xbox/ChonkyStation4.Xbox/MainPage.xaml.cpp`
- `platform/xbox/ChonkyStation4.Xbox/pch.h`
- `platform/xbox/ChonkyStation4.Xbox/ChonkyStation4.Xbox.vcxproj`
- `platform/xbox/ChonkyStation4.Xbox/ChonkyStation4.Xbox.vcxproj.filters`
- `platform/xbox/tools/validate_d3d12_foundation.py`
- `docs/M4_D3D12_BACKEND.md`

## Validation

### Project structure

```bash
python3 platform/xbox/tools/validate_project.py
```

Result: **PASS**.

### D3D12 static foundation

```bash
python3 platform/xbox/tools/validate_d3d12_foundation.py
```

Result: **PASS** for presence/wiring of device, adapter, queue, allocator, command list, composition swap chain, descriptor heaps, resources, shader path, fence synchronization, clear, triangle and textured-quad paths.

This is static validation only and does not substitute for compilation/runtime execution.

### UWP build

```powershell
msbuild platform\xbox\ChonkyStation4.Xbox.sln /m /t:Rebuild /p:Configuration=Release /p:Platform=x64
```

Result in current environment: **NOT TESTED / BLOCKED** — `msbuild` is not installed (`exit 127`).

### Render tests

- Clear screen: **NOT TESTED on Windows/Xbox**.
- Triangle: **NOT TESTED on Windows/Xbox**.
- Textured quad: **NOT TESTED on Windows/Xbox**.

## Milestone ledger

- Implementation: PASS by source/static validation
- UWP compile/link: NOT TESTED — Windows toolchain unavailable
- D3D12 device initialization runtime: NOT TESTED
- Clear/triangle/textured-quad runtime: NOT TESTED
- Xbox hardware: NOT TESTED
- Overall M4 status: **PARTIAL**
