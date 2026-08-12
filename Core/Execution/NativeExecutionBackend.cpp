#include "NativeExecutionBackend.hpp"

#include "Core/Execution/ExecutionContext.hpp"

namespace ChonkyStation4::Core::Execution {

const char* NativeExecutionBackend::Name() const noexcept {
    return "Native x86-64 execution boundary";
}

bool NativeExecutionBackend::IsAvailable() const noexcept {
    // The M11 boundary is available; actual native guest execution is a later
    // milestone and is intentionally not attempted here.
    return true;
}

ExecutionBoundaryResult NativeExecutionBackend::Start(ExecutionContext& context) const {
    ExecutionBoundaryResult result;
    result.backend = Name();
    result.available = IsAvailable();
    if (!context.IsValid()) {
        result.error = "execution context is not valid";
        result.message = result.error;
        return result;
    }

    result.accepted = true;
    result.message = "execution boundary reached; guest code execution is not enabled in M11";
    return result;
}

} // namespace ChonkyStation4::Core::Execution
