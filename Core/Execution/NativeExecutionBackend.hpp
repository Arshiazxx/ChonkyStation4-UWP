#pragma once

#include "Core/Execution/IExecutionBackend.hpp"

namespace ChonkyStation4::Core::Execution {

class NativeExecutionBackend final : public IExecutionBackend {
public:
    const char* Name() const noexcept override;
    bool IsAvailable() const noexcept override;
    ExecutionBoundaryResult Start(ExecutionContext& context) const override;
};

} // namespace ChonkyStation4::Core::Execution
