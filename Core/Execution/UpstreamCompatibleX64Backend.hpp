#pragma once

#include "Core/Execution/IExecutionBackend.hpp"

namespace ChonkyStation4::Core::Execution {

// Boundary for the upstream ChonkyStation4 execution strategy: validated
// host x86-64 module mappings, ABI/TLS setup, and a controlled entry transfer.
// This milestone reports the boundary only; it does not execute guest code.
class UpstreamCompatibleX64Backend final : public IExecutionBackend {
public:
    const char* Name() const noexcept override;
    bool IsAvailable() const noexcept override;
    ExecutionBoundaryResult Start(ExecutionContext& context) const override;
};

} // namespace ChonkyStation4::Core::Execution
