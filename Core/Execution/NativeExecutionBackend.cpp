#include "NativeExecutionBackend.hpp"

#include "Core/Execution/ExecutionContext.hpp"
#include "Core/Execution/UpstreamCompatibleX64Backend.hpp"

namespace ChonkyStation4::Core::Execution {

const char* NativeExecutionBackend::Name() const noexcept {
    return "Upstream native x86-64 execution backend";
}

bool NativeExecutionBackend::IsAvailable() const noexcept {
    return UpstreamCompatibleX64Backend{}.IsAvailable();
}

ExecutionBoundaryResult NativeExecutionBackend::Start(ExecutionContext& context) const {
    ExecutionBoundaryResult result;
    const auto upstreamResult = UpstreamCompatibleX64Backend{}.Start(context);
    result = upstreamResult;
    result.backend = Name();
    return result;
}

} // namespace ChonkyStation4::Core::Execution
