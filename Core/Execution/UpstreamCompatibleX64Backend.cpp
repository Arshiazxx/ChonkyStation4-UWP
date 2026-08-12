#include "UpstreamCompatibleX64Backend.hpp"

#include "Core/Execution/ExecutionContext.hpp"

namespace ChonkyStation4::Core::Execution {

const char* UpstreamCompatibleX64Backend::Name() const noexcept {
    return "Upstream native x86-64 execution backend";
}

bool UpstreamCompatibleX64Backend::IsAvailable() const noexcept {
    return backend_.IsAvailable();
}

ExecutionBoundaryResult UpstreamCompatibleX64Backend::Start(ExecutionContext& context) const {
    auto result = backend_.Start(context);
    result.backend = Name();
    return result;
}

} // namespace ChonkyStation4::Core::Execution
