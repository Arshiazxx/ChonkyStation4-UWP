#pragma once

#include "Core/Kernel/Exceptions/ExceptionBoundary.hpp"
#include "Core/Kernel/Thread/Thread.hpp"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace ChonkyStation4::Core::Kernel {

struct SchedulerRunReport {
    bool success = false;
    std::uint64_t threadsRun = 0;
    std::uint64_t instructionsExecuted = 0;
    std::string log;
    std::string error;
};

class Scheduler final {
public:
    Scheduler();

    Thread* CreateThread(
        Process& process,
        GuestVirtualAddress entryPoint,
        GuestVirtualAddress stackBase,
        std::uint64_t stackSize,
        std::string* error = nullptr);

    SchedulerRunReport Run(
        Process& process,
        std::uint64_t maxInstructionsPerThread = 10000);

    Thread* FindThread(ThreadId id) noexcept;
    const Thread* FindThread(ThreadId id) const noexcept;

    const std::vector<std::unique_ptr<Thread>>& Threads() const noexcept;

private:
    DefaultExceptionHandler defaultExceptionHandler_;
    ExceptionBoundary exceptionBoundary_;
    ThreadId nextThreadId_ = 1;
    std::vector<std::unique_ptr<Thread>> threads_;
};

} // namespace ChonkyStation4::Core::Kernel
