#include "UpstreamCompatibleX64Backend.hpp"

#include "Core/Execution/ExecutionContext.hpp"

namespace ChonkyStation4::Core::Execution {

const char* UpstreamCompatibleX64Backend::Name() const noexcept {
    return "Upstream-compatible x86-64 execution boundary";
}

bool UpstreamCompatibleX64Backend::IsAvailable() const noexcept {
#if defined(_M_X64) || defined(__x86_64__) || defined(__amd64__)
    return true;
#else
    return false;
#endif
}

ExecutionBoundaryResult UpstreamCompatibleX64Backend::Start(ExecutionContext& context) const {
    ExecutionBoundaryResult result;
    result.backend = Name();
    result.available = IsAvailable();
    if (!result.available) {
        result.error = "upstream-compatible backend requires an x86-64 host";
        result.message = result.error;
        return result;
    }
    if (!context.IsValid()) {
        result.error = "execution context is not valid";
        result.message = result.error;
        return result;
    }

    result.accepted = true;
    result.message =
        "upstream-compatible x86-64 boundary available; guest execution is not enabled";
    return result;
}

} // namespace ChonkyStation4::Core::Execution
