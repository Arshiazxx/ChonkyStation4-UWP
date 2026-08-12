#include "AbiSmokeTests.hpp"

#include "Core/ABI/Arguments/AbiContext.hpp"
#include "Core/ABI/CallingConvention/X86_64CallingConvention.hpp"
#include "Core/ABI/ReturnValues/ReturnValue.hpp"
#include "Core/ABI/Syscall/SyscallTransition.hpp"
#include "Core/Kernel/Process/Process.hpp"
#include "Core/Kernel/Scheduler/Scheduler.hpp"
#include "Core/Kernel/Syscalls/SyscallDispatcher.hpp"
#include "Core/Kernel/Thread/Thread.hpp"

#include <cstdint>
#include <limits>
#include <sstream>

namespace ChonkyStation4::Core::ABI {

namespace {

constexpr std::uint64_t EntryPoint = 0x400000;
constexpr std::uint64_t InitialStack = 0x600000;
constexpr std::uint64_t CreatedThreadStack = 0x610000;

bool Check(bool condition, const char* message, std::string& failure) {
    if (condition) {
        return true;
    }
    failure = message;
    return false;
}

void SetSyscall(
    AbiContext& abi,
    Kernel::Syscall syscall) {
    abi.SetSyscallNumber(static_cast<std::uint64_t>(syscall));
}

void SetRawSyscall(
    AbiContext& abi,
    std::uint64_t syscall) {
    abi.SetSyscallNumber(syscall);
}

bool SetArgument(
    AbiContext& abi,
    std::size_t index,
    std::uint64_t value,
    const char* message,
    std::string& failure) {
    if (abi.TrySetArgument(index, value)) {
        return true;
    }
    failure = message;
    return false;
}

} // namespace

AbiSmokeTestReport RunAbiSmokeTests() {
    AbiSmokeTestReport report;
    std::ostringstream log;
    std::string failure;

    log << "ChonkyStation4 ABI Test\n\n";

    Memory::GuestMemory memory;
    Kernel::Process process(1, memory);
    Kernel::Scheduler scheduler;
    std::string error;
    auto* initialThread = scheduler.CreateThread(
        process, EntryPoint, InitialStack, 0x1000, &error);
    if (!Check(initialThread != nullptr, "initial thread creation failed", failure)) {
        report.failure = failure + (error.empty() ? "" : ": " + error);
        report.log = log.str() + "ABI smoke tests:\nFAIL\n" + report.failure + "\n";
        return report;
    }

    AbiContext abi(initialThread->Cpu());

    Kernel::SyscallDispatcher dispatcher;
    SyscallTransition transition;
    if (!Check(dispatcher.Registry().Size() >= 7,
               "PS4 syscall registry did not contain the initial stubs", failure) ||
        !Check(dispatcher.Metadata(static_cast<Kernel::SyscallNumber>(
                   Kernel::Syscall::ThreadCreate)) != nullptr,
               "thread-create syscall metadata lookup failed", failure)) {
        report.failure = failure;
        report.log = log.str() + "ABI smoke tests:\nFAIL\n" + report.failure + "\n";
        return report;
    }

    for (std::size_t index = 0;
         index < X86_64CallingConvention::RegisterArgumentCount;
         ++index) {
        const auto expected = 0x1000ull + static_cast<std::uint64_t>(index);
        if (!Check(X86_64CallingConvention::TrySetArgument(
                       initialThread->Cpu(), index, expected),
                   "ABI argument register write failed", failure)) {
            break;
        }
        std::uint64_t actual = 0;
        if (!Check(X86_64CallingConvention::TryGetArgument(
                       initialThread->Cpu(), index, actual),
                   "ABI argument register read failed", failure) ||
            !Check(actual == expected,
                   "ABI argument register mapping returned the wrong value", failure)) {
            break;
        }
    }
    if (failure.empty()) {
        std::uint64_t ignored = 0;
        if (!Check(!abi.TryGetArgument(
                       X86_64CallingConvention::RegisterArgumentCount, ignored),
                   "ABI rejected argument-register overflow lookup", failure) ||
            !Check(!abi.TrySetArgument(
                       X86_64CallingConvention::RegisterArgumentCount, 0),
                   "ABI rejected argument-register overflow write", failure)) {
            // Keep the first ABI mapping failure for the final report.
        }
    }
    if (!failure.empty()) {
        report.failure = failure;
        report.log = log.str() + "ABI smoke tests:\nFAIL\n" + report.failure + "\n";
        return report;
    }

    SetSyscall(abi, Kernel::Syscall::ThreadCreate);
    if (!SetArgument(abi, 0, EntryPoint,
                     "thread-create entry-point argument write failed", failure) ||
        !SetArgument(abi, 1, CreatedThreadStack,
                     "thread-create stack argument write failed", failure) ||
        !SetArgument(abi, 2, 0x1000,
                     "thread-create stack-size argument write failed", failure)) {
        report.failure = failure;
        report.log = log.str() + "ABI smoke tests:\nFAIL\n" + report.failure + "\n";
        return report;
    }
    log << "Calling:\nsceKernelCreateThread\n\n";
    const auto create = transition.Invoke(
        process, *initialThread, dispatcher, &scheduler);
    auto* createdThread = scheduler.FindThread(2);
    log << "Result:\n" << (create.success ? "SUCCESS" : "FAILURE")
        << "\n\nReturn:\n";
    if (create.success) {
        log << "Thread ID " << ReadReturnValue(initialThread->Cpu()).raw << "\n\n";
    } else {
        log << "ERROR\n\n";
    }
    log << "Transition trace:\n" << create.log << '\n';
    if (!Check(create.success, "sceKernelCreateThread did not succeed", failure) ||
        !Check(createdThread != nullptr, "created thread was not registered", failure) ||
        !Check(create.result.value == 2,
               "sceKernelCreateThread returned the wrong thread ID", failure) ||
        !Check(ReadReturnValue(initialThread->Cpu()).raw == create.result.value,
               "ABI return register did not contain the new thread ID", failure)) {
        report.failure = failure;
    }

    if (failure.empty()) {
        SetSyscall(abi, Kernel::Syscall::ProcessGetInfo);
        const auto processInfo = transition.Invoke(
            process, *initialThread, dispatcher, &scheduler);
        log << processInfo.log << '\n';
        if (!Check(processInfo.success, "process-info syscall failed", failure) ||
            !Check(processInfo.result.value == process.Id(),
                   "process-info syscall returned the wrong process ID", failure) ||
            !Check(ReadReturnValue(initialThread->Cpu()).raw == processInfo.result.value,
                   "process-info return value was incorrect", failure)) {
            report.failure = failure;
        }
    }

    if (failure.empty()) {
        SetRawSyscall(abi, 0xFFFF);
        const auto unknown = transition.Invoke(
            process, *initialThread, dispatcher, &scheduler);
        log << unknown.log << '\n';
        if (!Check(!unknown.success, "unknown syscall unexpectedly succeeded", failure) ||
            !Check(ReadReturnValue(initialThread->Cpu()).raw ==
                       (std::numeric_limits<std::uint64_t>::max)(),
                   "unknown syscall did not return the ABI error value", failure)) {
            report.failure = failure;
        }
    }

    std::uint64_t allocation = 0;
    if (failure.empty()) {
        SetSyscall(abi, Kernel::Syscall::MemoryAllocate);
        if (!SetArgument(abi, 0, 0x2000,
                         "memory-allocation size argument write failed", failure)) {
            report.failure = failure;
        }
    }
    if (failure.empty()) {
        const auto allocate = transition.Invoke(
            process, *initialThread, dispatcher, &scheduler);
        allocation = ReadReturnValue(initialThread->Cpu()).raw;
        log << allocate.log << '\n';
        if (!Check(allocate.success, "memory allocation syscall failed", failure) ||
            !Check(allocate.result.value != 0,
                   "memory allocation returned an invalid address", failure) ||
            !Check(allocation == allocate.result.value,
                   "memory allocation return register was incorrect", failure)) {
            report.failure = failure;
        }
    }

    if (failure.empty()) {
        SetSyscall(abi, Kernel::Syscall::MemoryRelease);
        if (!SetArgument(abi, 0, allocation,
                         "memory-release address argument write failed", failure)) {
            report.failure = failure;
        }
    }
    if (failure.empty()) {
        const auto release = transition.Invoke(
            process, *initialThread, dispatcher, &scheduler);
        log << release.log << '\n';
        if (!Check(release.success, "memory release syscall failed", failure) ||
            !Check(release.result.value == 0,
                   "memory release returned a non-zero result", failure) ||
            !Check(ReadReturnValue(initialThread->Cpu()).raw == 0,
                   "memory release return register was incorrect", failure)) {
            report.failure = failure;
        }
    }

    if (failure.empty()) {
        SetSyscall(abi, Kernel::Syscall::ThreadStart);
        if (!SetArgument(abi, 0, createdThread->Id(),
                         "thread-start ID argument write failed", failure)) {
            report.failure = failure;
        }
    }
    if (failure.empty()) {
        const auto start = transition.Invoke(
            process, *initialThread, dispatcher, &scheduler);
        log << start.log << '\n';
        if (!Check(start.success, "thread-start syscall failed", failure) ||
            !Check(start.result.value == createdThread->Id(),
                   "thread-start returned the wrong thread ID", failure) ||
            !Check(ReadReturnValue(initialThread->Cpu()).raw == start.result.value,
                   "thread-start return register was incorrect", failure)) {
            report.failure = failure;
        }
    }

    if (failure.empty()) {
        AbiContext createdThreadAbi(createdThread->Cpu());
        SetSyscall(createdThreadAbi, Kernel::Syscall::ThreadExit);
        if (!SetArgument(createdThreadAbi, 0, 7,
                         "thread-exit code argument write failed", failure)) {
            report.failure = failure;
        }
        if (!failure.empty()) {
            report.log = log.str() + "ABI smoke tests:\nFAIL\n" + report.failure + "\n";
            return report;
        }
        const auto exitThread = transition.Invoke(
            process, *createdThread, dispatcher, &scheduler);
        log << exitThread.log << '\n';
        if (!Check(exitThread.success, "thread-exit syscall failed", failure) ||
            !Check(exitThread.result.value == 0,
                   "thread-exit returned a non-zero result", failure) ||
            !Check(ReadReturnValue(createdThread->Cpu()).raw == 0,
                   "thread-exit return register was incorrect", failure) ||
            !Check(createdThread->State() == Kernel::ThreadState::Exited,
                   "thread-exit syscall did not update thread state", failure) ||
            !Check(createdThread->ExitCode() == 7,
                   "thread-exit did not receive the exit code argument", failure)) {
            report.failure = failure;
        }
    }

    if (failure.empty()) {
        SetSyscall(abi, Kernel::Syscall::ProcessExit);
        if (!SetArgument(abi, 0, 0,
                         "process-exit code argument write failed", failure)) {
            report.failure = failure;
        }
    }
    if (failure.empty()) {
        const auto exitProcess = transition.Invoke(
            process, *initialThread, dispatcher, &scheduler);
        log << exitProcess.log << '\n';
        if (!Check(exitProcess.success, "process-exit syscall failed", failure) ||
            !Check(exitProcess.result.value == 0,
                   "process-exit returned a non-zero result", failure) ||
            !Check(ReadReturnValue(initialThread->Cpu()).raw == 0,
                   "process-exit return register was incorrect", failure) ||
            !Check(process.State() == Kernel::ProcessState::Terminated,
                   "process-exit syscall did not terminate the process", failure)) {
            report.failure = failure;
        }
    }

    if (report.failure.empty()) {
        report.passed = true;
        log << "ABI smoke tests:\nPASS\n";
    } else {
        log << "ABI smoke tests:\nFAIL\n" << report.failure << '\n';
    }
    report.log = log.str();
    return report;
}

} // namespace ChonkyStation4::Core::ABI
