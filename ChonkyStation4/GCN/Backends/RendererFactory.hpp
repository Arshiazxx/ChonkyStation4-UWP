#pragma once

#include <memory>

namespace PS4::GCN {

class Renderer;

enum class RendererBackend {
    Vulkan,
};

// Central renderer construction seam. Desktop behavior defaults to Vulkan;
// additional host backends (for example D3D12) can be added without coupling
// GCN command processing to a concrete renderer implementation.
std::unique_ptr<Renderer> createRenderer(RendererBackend backend = RendererBackend::Vulkan);

} // namespace PS4::GCN
