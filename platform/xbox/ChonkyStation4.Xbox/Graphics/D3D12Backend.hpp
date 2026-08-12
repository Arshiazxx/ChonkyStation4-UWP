#pragma once

#include <memory>
#include <string>

struct IUnknown;

namespace ChonkyStation4 {
namespace Xbox {
namespace Graphics {

enum class DemoScene {
    Clear,
    Triangle,
    TexturedQuad,
};

// Host-side D3D12 foundation. This intentionally does not implement the PS4
// Renderer interface yet; M4 only proves device/swap-chain/resource/pipeline
// mechanics needed by a future emulator backend.
class D3D12Backend final {
public:
    D3D12Backend();
    ~D3D12Backend();

    D3D12Backend(const D3D12Backend&) = delete;
    D3D12Backend& operator=(const D3D12Backend&) = delete;

    bool Initialize(IUnknown* swapChainPanel, unsigned width, unsigned height);
    void Shutdown();
    bool Render(DemoScene scene);

    bool IsInitialized() const;
    const std::wstring& Status() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace Graphics
} // namespace Xbox
} // namespace ChonkyStation4
