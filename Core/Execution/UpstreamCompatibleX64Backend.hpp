#pragma once

#include "Core/Execution/IExecutionBackend.hpp"
#include "Core/Execution/Upstream/UpstreamExecutionBackend.hpp"

namespace ChonkyStation4::Core::Execution {

// Compatibility name retained for M11/M12 callers. The implementation now
// delegates to the upstream execution source adapter and its Xbox/UWP
// platform gate. It remains safe until a real native entry bridge is enabled.
class UpstreamCompatibleX64Backend final : public IExecutionBackend {
public:
    const char* Name() const noexcept override;
    bool IsAvailable() const noexcept override;
    ExecutionBoundaryResult Start(ExecutionContext& context) const override;

private:
    UpstreamExecutionBackend backend_;
};

} // namespace ChonkyStation4::Core::Execution
