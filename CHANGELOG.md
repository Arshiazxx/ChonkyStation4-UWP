# Changelog

This changelog records the Xbox/UWP port foundation layered on authoritative ChonkyStation4 upstream commit `310269290a3c256f5911d4bc7e441489bffffbf6`.

## M6 — Guest memory foundation

- Added `platform/xbox/Memory/XboxGuestMemory.*`.
- Mirrored current upstream guest memory constants: base `0x80000000`, 2048 GiB reservation, 2000 GiB search range, system mapping area `0x1000000000`, and 16 KiB guest page contract.
- Added UWP-aware reserve/commit/protect/decommit/release data-memory primitives.
- Added pagefile-backed `CreateFileMappingFromApp` / `MapViewOfFileFromApp` diagnostic.
- Added exact-layout guest-memory diagnostic to `XboxCapabilityProbe`.
- Added checked-in `xbox-guest-memory.json` NOT_RUN baseline.
- Explicitly excluded executable memory and guest execution from M6.

## M5 — Xbox capability probe

- Added standalone `XboxCapabilityProbe` UWP project.
- Added memory, virtual-memory, threading, graphics and UWP capability probes.
- Added bounded allocation/descriptor/resource tests.
- Added D3D12 feature-level, shader-model and format queries.
- Added LocalFolder, Gamepad, network-profile and audio-endpoint probes.
- Added W→RX executable-memory feasibility check without invoking generated guest code.
- Added `codeGeneration` capability required by the probe's executable-protection diagnostics.
- Added checked-in `xbox-capabilities.json` NOT_RUN baseline.
- Added static probe validator.

## M4 — D3D12 backend foundation

- Added host-side `D3D12Backend` for the UWP diagnostic host.
- Added hardware adapter selection and D3D12 device creation.
- Added command queue, allocator, command list and composition swap chain.
- Added RTV and shader-visible SRV descriptor heaps.
- Added vertex/texture/upload resource foundations.
- Added root signature, diagnostic shader compilation and graphics pipelines.
- Added fence synchronization.
- Added clear, triangle and textured-quad render modes.
- Did not register D3D12 as a PS4 renderer; PS4-facing backend semantics remain future work.

## M3 — Xbox UWP host

- Added standalone x64 C++ UWP/XAML solution under `platform/xbox/ChonkyStation4.Xbox`.
- Added `Windows.Xbox` target manifest.
- Added app launch/suspend/resume lifecycle handling.
- Added diagnostic UI, logging, LocalFolder access and Gamepad count/events.
- Added `SwapChainPanel` graphics host surface.
- Added repository-side project/manifest validator.
- The emulator core is deliberately not launched by M3.

## M2 — Platform boundary extraction

- Added `RendererFactory` as the renderer construction seam.
- Preserved Vulkan as the default and only emulator renderer.
- Removed SDL ownership from the generic `Renderer` interface.
- Moved SDL window ownership to `VulkanRenderer`.
- Added virtual destructor to `Renderer`.
- Left GCN PM4 decoding and shader/GPU emulation behavior unchanged.

## M1 — Architecture audit

- Added current-upstream architecture and Xbox integration analysis.
- Identified separate platform-service, graphics, guest-memory and native-execution gates.

## M0 — Upstream migration baseline

- Established `liuk7071/ChonkyStation4` as authoritative.
- Verified supplied source archive against commit `310269290a3c256f5911d4bc7e441489bffffbf6`.
- Documented the public fork relationship and classified legacy Xbox/UWP reference work.
