#include "UpstreamExecutionBackend.hpp"

#include "Core/Execution/ExecutionContext.hpp"

namespace ChonkyStation4::Core::Execution {

const char* UpstreamExecutionBackend::Name() const noexcept {
    return "Upstream native x86-64 execution backend";
}

bool UpstreamExecutionBackend::IsAvailable() const noexcept {
    return platform_.Probe().x64Host;
}

ExecutionBoundaryResult UpstreamExecutionBackend::Start(ExecutionContext& context) const {
    ExecutionBoundaryResult result;
    result.backend = Name();

    const auto capabilities = platform_.Probe();
    result.available = capabilities.x64Host;
    if (!result.available) {
        result.error = "upstream execution requires an x86-64 host";
        result.message = result.error;
        return result;
    }
    if (!context.IsValid()) {
        result.error = "execution context is not valid";
        result.message = result.error;
        return result;
    }

    result.accepted = true;
    Upstream::EntryState state;
    state.nativeEntryPoint = context.NativeEntryPoint();
    state.stackPointer = context.StackPointer();
    state.parameterBlock = context.GeneralRegisters()[CPU::RegisterId::Rdi];
    state.exitHandler = context.GeneralRegisters()[CPU::RegisterId::Rsi];
    if (const auto* mainModule = context.ProcessReference().MainModule();
        mainModule != nullptr) {
        state.moduleName = mainModule->Name();
    }

    if (state.nativeEntryPoint == 0) {
        result.message =
            "upstream entry state prepared; no host-mapped native entry point is installed";
        return result;
    }

    const auto transfer = platform_.TransferToEntry(state);
    result.executed = transfer.completed;
    if (!transfer.error.empty()) {
        result.error = transfer.error;
    }
    result.message = transfer.message;
    return result;
}

const Upstream::XboxUwpExecutionPlatform&
UpstreamExecutionBackend::Platform() const noexcept {
    return platform_;
}

} // namespace ChonkyStation4::Core::Execution
