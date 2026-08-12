#include "RendererFactory.hpp"

#include <GCN/Backends/Renderer.hpp>
#include <GCN/Backends/Vulkan/VulkanRenderer.hpp>

#include <stdexcept>

namespace PS4::GCN {

std::unique_ptr<Renderer> createRenderer(RendererBackend backend) {
    switch (backend) {
    case RendererBackend::Vulkan:
        return std::make_unique<Vulkan::VulkanRenderer>();
    }

    throw std::runtime_error("Unsupported renderer backend");
}

} // namespace PS4::GCN
