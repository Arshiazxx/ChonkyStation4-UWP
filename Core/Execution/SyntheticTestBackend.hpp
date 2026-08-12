#pragma once

#include "Core/Execution/IExecutionBackend.hpp"

namespace ChonkyStation4::Core::Execution {

// Adapter that makes the existing M8 synthetic executor selectable through
// the common execution-backend boundary. It never decodes or executes real
// PS4/x86-64 instructions.
class SyntheticTestBackend final : public IExecutionBackend {
public:
    const char* Name() const noexcept override;
    bool IsAvailable() const noexcept override;
    ExecutionBoundaryResult Start(ExecutionContext& context) const override;
};

} // namespace ChonkyStation4::Core::Execution
