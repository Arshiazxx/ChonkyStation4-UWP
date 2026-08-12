#include "Scheduler.hpp"

#include "Core/CPU/CpuExecutor.hpp"

#include <sstream>

namespace ChonkyStation4::Core::Kernel {

namespace {

void AppendLine(std::ostringstream& log, const std::string& line) {
    log << line << '\n';
}

} // namespace

Scheduler::Scheduler()
    : exceptionBoundary_(defaultExceptionHandler_) {}

Thread* Scheduler::CreateThread(
    Process& process,
    GuestVirtualAddress entryPoint,
    GuestVirtualAddress stackBase,
    std::uint64_t stackSize,
    std::string* error) {
    if (process.State() == ProcessState::Terminated ||
        process.State() == ProcessState::Faulted) {
        if (error != nullptr) {
            *error = "cannot create a thread in a terminated or faulted process";
        }
        return nullptr;
    }
    if (nextThreadId_ == 0) {
        if (error != nullptr) {
            *error = "thread identifier space is exhausted";
        }
        return nullptr;
    }

    auto thread = std::make_unique<Thread>(
        nextThreadId_++, process, entryPoint, stackBase, stackSize);
    auto* result = thread.get();
    process.AttachThread(result->Id());
    threads_.push_back(std::move(thread));
    return result;
}

SchedulerRunReport Scheduler::Run(
    Process& process,
    std::uint64_t maxInstructionsPerThread) {
    SchedulerRunReport report;
    std::ostringstream log;
    AppendLine(log, "Execution started");

    if (process.State() == ProcessState::Terminated ||
        process.State() == ProcessState::Faulted) {
        report.error = "process cannot be scheduled in its current state";
        AppendLine(log, "Scheduler error: " + report.error);
        report.log = log.str();
        return report;
    }

    process.SetState(ProcessState::Running);
    bool encounteredFault = false;
    for (const auto& ownedThread : threads_) {
        auto& thread = *ownedThread;
        if (&thread.OwnerProcess() != &process ||
            (thread.State() != ThreadState::Created &&
             thread.State() != ThreadState::Ready)) {
            continue;
        }

        thread.MarkRunning();
        CPU::CpuExecutor executor(process.AddressSpace());
        const auto execution = executor.Run(thread.Cpu(), maxInstructionsPerThread);
        report.threadsRun++;
        report.instructionsExecuted += execution.instructionsExecuted;
        log << execution.log;

        if (execution.success) {
            thread.MarkHalted();
            continue;
        }

        encounteredFault = true;
        report.error = execution.error;
        std::string exceptionLog;
        exceptionBoundary_.HandleCpuFailure(process, thread, execution, &exceptionLog);
        log << exceptionLog;
        break;
    }

    report.success = !encounteredFault && report.threadsRun != 0;
    if (!encounteredFault && process.State() == ProcessState::Running) {
        process.SetState(ProcessState::Ready);
    }
    report.log = log.str();
    return report;
}

Thread* Scheduler::FindThread(ThreadId id) noexcept {
    for (const auto& thread : threads_) {
        if (thread->Id() == id) {
            return thread.get();
        }
    }
    return nullptr;
}

const Thread* Scheduler::FindThread(ThreadId id) const noexcept {
    for (const auto& thread : threads_) {
        if (thread->Id() == id) {
            return thread.get();
        }
    }
    return nullptr;
}

const std::vector<std::unique_ptr<Thread>>& Scheduler::Threads() const noexcept {
    return threads_;
}

} // namespace ChonkyStation4::Core::Kernel
