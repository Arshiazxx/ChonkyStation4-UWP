#pragma once

#include "Core/Execution/IExecutionBackend.hpp"
#include "Core/Execution/Upstream/XboxUwpExecutionPlatform.hpp"

namespace ChonkyStation4::Core::Execution {

// Implements the upstream execution sequence at the backend boundary:
// validate an x86-64 context, assemble entry state, then ask the platform
// adapter to perform the native entry transfer. It never interprets guest
// instructions and never treats a guest virtual address as a host pointer.
class UpstreamExecutionBackend final : public IExecutionBackend {
public:
    const char* Name() const noexcept override;
    bool IsAvailable() const noexcept override;
    ExecutionBoundaryResult Start(ExecutionContext& context) const override;

    const Upstream::XboxUwpExecutionPlatform& Platform() const noexcept;

private:
    Upstream::XboxUwpExecutionPlatform platform_;
};

} // namespace ChonkyStation4::Core::Execution
