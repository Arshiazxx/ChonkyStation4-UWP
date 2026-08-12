# D3D12 Backend Foundation

Authoritative emulator baseline: `liuk7071/ChonkyStation4` at `310269290a3c256f5911d4bc7e441489bffffbf6`.

This is the canonical overview of the M4 D3D12 work. Detailed milestone validation is in `M4_D3D12_BACKEND.md`.

## Scope

The D3D12 implementation is a **host-side foundation and diagnostic renderer**, not yet a PS4 GPU backend.

```text
Xbox/UWP host
    -> SwapChainPanel
        -> D3D12Backend
            -> DXGI hardware adapter
            -> D3D12 device
            -> command queue / allocator / list
            -> composition swap chain
            -> RTV/SRV descriptor heaps
            -> resources
            -> root signature / pipeline state
            -> fence synchronization
            -> Clear / Triangle / TexturedQuad diagnostics
```

The current PS4 GPU path remains:

```text
GCN PM4 processing
    -> Renderer interface
        -> RendererFactory
            -> VulkanRenderer
```

M4 deliberately does not add a fake `D3D12Renderer : Renderer`. The PS4-facing methods (`draw`, `dispatch`, `flip`, resource translation, shader translation, synchronization semantics) must be implemented only when they have real emulator semantics.

## Implemented foundation

- hardware DXGI adapter enumeration; software/WARP adapters are skipped;
- D3D12 device creation and feature-level query;
- command queue, allocator, graphics command list;
- `CreateSwapChainForComposition` bound to XAML `SwapChainPanel`;
- render-target descriptor heap and swap-chain back buffers;
- shader-visible SRV descriptor heap;
- committed vertex/texture/upload resources;
- root signature and two diagnostic graphics PSOs;
- runtime HLSL compilation for diagnostic shaders;
- fence/event synchronization;
- explicit PRESENT ↔ RENDER_TARGET state transitions;
- clear-screen diagnostic;
- triangle diagnostic;
- textured-quad diagnostic.

## Current limitations

- Not compiled in this execution environment because MSBuild/MSVC/Windows SDK are unavailable.
- Not run on Windows UWP or Xbox hardware.
- No PS4 GCN command implementation uses D3D12 yet.
- No GCN-to-HLSL/DXIL shader translation exists yet.
- No PS4 texture/buffer/cache semantics have been ported from Vulkan.
- No Xbox-specific performance assumptions are made.

## Future PS4 GPU integration point

The M2 renderer factory is the only intended GCN-side selection seam:

```text
GCN
  -> RendererFactory
       |-> VulkanRenderer   (existing)
       `-> D3D12Renderer    (future real PS4 backend)
```

A future D3D12 PS4 backend should reuse the proven M4 device/resource/synchronization mechanisms where appropriate, but must implement the existing `Renderer` contract rather than modifying PM4 decoding to call D3D12 directly.

## Xbox considerations

Runtime D3D12 feature level, shader model, format support, descriptor allocation behavior, and resource limits are intentionally measured by `XboxCapabilityProbe` rather than assumed. See `XBOX_CAPABILITIES.md` and the generated `xbox-capabilities.json`.

## Validation status

**PARTIAL** — repository/static validation passes; Windows/UWP compilation and rendering are **NOT TESTED** in the current Linux environment.
