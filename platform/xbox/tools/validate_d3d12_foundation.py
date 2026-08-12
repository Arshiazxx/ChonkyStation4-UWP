#!/usr/bin/env python3
"""Static validation for the M4 D3D12 foundation.

This verifies the architectural/render-test pieces are present. It cannot prove
Windows SDK compilation or GPU runtime behavior.
"""
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]
HOST = ROOT / "ChonkyStation4.Xbox"
CPP = HOST / "Graphics" / "D3D12Backend.cpp"
HPP = HOST / "Graphics" / "D3D12Backend.hpp"
PROJECT = HOST / "ChonkyStation4.Xbox.vcxproj"
MAIN = HOST / "MainPage.xaml.cpp"

checks = {
    "device creation": "D3D12CreateDevice",
    "adapter selection": "EnumAdapters1",
    "command queue": "CreateCommandQueue",
    "command allocator": "CreateCommandAllocator",
    "command list": "CreateCommandList",
    "composition swap chain": "CreateSwapChainForComposition",
    "XAML swap-chain binding": "ISwapChainPanelNative::SetSwapChain",
    "RTV descriptor heap": "D3D12_DESCRIPTOR_HEAP_TYPE_RTV",
    "shader-visible SRV heap": "D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV",
    "resource allocation": "CreateCommittedResource",
    "shader compilation": "D3DCompile",
    "root signature": "CreateRootSignature",
    "pipeline state": "CreateGraphicsPipelineState",
    "fence synchronization": "CreateFence",
    "presentation": "Present(1, 0)",
    "clear test": "ClearRenderTargetView",
    "triangle/quad draw": "DrawInstanced",
    "texture SRV": "CreateShaderResourceView",
}

text = CPP.read_text()
for label, token in checks.items():
    if token not in text:
        print(f"ERROR: missing {label}: {token}", file=sys.stderr)
        raise SystemExit(1)
    print(f"PASS: {label}")

header = HPP.read_text()
for scene in ("Clear", "Triangle", "TexturedQuad"):
    if scene not in header:
        print(f"ERROR: missing demo scene {scene}", file=sys.stderr)
        raise SystemExit(1)
print("PASS: clear/triangle/textured-quad demo modes")

project = PROJECT.read_text()
for lib in ("d3d12.lib", "dxgi.lib", "d3dcompiler.lib"):
    if lib not in project:
        print(f"ERROR: project does not link {lib}", file=sys.stderr)
        raise SystemExit(1)
print("PASS: D3D12/DXGI/shader compiler link dependencies")

main = MAIN.read_text()
for handler in ("OnRenderClear", "OnRenderTriangle", "OnRenderTexturedQuad"):
    if handler not in main:
        print(f"ERROR: host missing {handler}", file=sys.stderr)
        raise SystemExit(1)
print("PASS: host render-test controls wired")

print("PASS: M4 static D3D12 foundation validation")
