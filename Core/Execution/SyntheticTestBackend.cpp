#include "SyntheticTestBackend.hpp"

#include "Core/CPU/CpuExecutor.hpp"
#include "Core/Execution/ExecutionContext.hpp"

namespace ChonkyStation4::Core::Execution {

const char* SyntheticTestBackend::Name() const noexcept {
    return "Synthetic M8 test backend";
}

bool SyntheticTestBackend::IsAvailable() const noexcept {
    return true;
}

ExecutionBoundaryResult SyntheticTestBackend::Start(ExecutionContext& context) const {
    ExecutionBoundaryResult result;
    result.backend = Name();
    result.available = IsAvailable();
    if (!context.IsValid()) {
        result.error = "execution context is not valid";
        result.message = result.error;
        return result;
    }

    CPU::CpuExecutor executor(context.ProcessReference().AddressSpace());
    const auto execution = executor.Run(context.CpuState());
    context.ApplyToThread();
    result.accepted = true;
    result.executed = execution.success;
    if (!execution.success) {
        result.error = execution.error.empty()
            ? "synthetic backend execution failed"
            : execution.error;
        result.message = result.error;
        return result;
    }

    result.message = "synthetic M8 program completed";
    return result;
}

} // namespace ChonkyStation4::Core::Execution
