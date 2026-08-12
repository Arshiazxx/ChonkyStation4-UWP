#include "RuntimeSmokeTests.hpp"

#include "Core/CPU/Instruction.hpp"
#include "Core/Kernel/Exceptions/ExceptionBoundary.hpp"
#include "Core/Kernel/Scheduler/Scheduler.hpp"
#include "Core/Kernel/Syscalls/SyscallDispatcher.hpp"

#include <cstdint>
#include <sstream>
#include <vector>

namespace ChonkyStation4::Core::Kernel {

namespace {

constexpr std::uint64_t CodeAddress = 0x400000;
constexpr std::uint64_t StackAddress = 0x600000;
constexpr std::uint64_t InvalidAddress = 0x700000;

std::uint32_t Permissions(
    Memory::MemoryPermission first,
    Memory::MemoryPermission second) {
    return Memory::ToPermissions(first) | Memory::ToPermissions(second);
}

void AppendU64(std::vector<std::uint8_t>& bytes, std::uint64_t value) {
    for (std::size_t index = 0; index < sizeof(value); ++index) {
        bytes.push_back(static_cast<std::uint8_t>(value >> (index * 8)));
    }
}

void AppendMovImmediate(
    std::vector<std::uint8_t>& bytes,
    CPU::RegisterId destination,
    std::uint64_t value) {
    bytes.push_back(static_cast<std::uint8_t>(CPU::Opcode::MovImmediate));
    bytes.push_back(static_cast<std::uint8_t>(destination));
    AppendU64(bytes, value);
}

bool Check(bool condition, const char* message, std::string& failure) {
    if (condition) {
        return true;
    }
    failure = message;
    return false;
}

} // namespace

RuntimeSmokeTestReport RunRuntimeSmokeTests() {
    RuntimeSmokeTestReport report;
    std::ostringstream log;
    std::string failure;

    log << "ChonkyStation4 Runtime\n\n";

    std::vector<std::uint8_t> program;
    AppendMovImmediate(program, CPU::RegisterId::Rax, 0);
    program.push_back(static_cast<std::uint8_t>(CPU::Opcode::Halt));

    Memory::GuestMemory memory;
    std::string memoryError;
    const Memory::MemoryRegion codeRegion{
        CodeAddress,
        0x1000,
        Permissions(Memory::MemoryPermission::Read, Memory::MemoryPermission::Execute),
        "runtime synthetic program",
    };
    if (!Check(memory.Map(codeRegion, program.data(), program.size(), &memoryError),
               "unable to map runtime synthetic program", failure)) {
        report.failure = failure + ": " + memoryError;
        report.log = log.str() + "Runtime smoke tests:\nFAIL\n" + report.failure + "\n";
        return report;
    }

    Process process(1, memory);
    if (!Check(process.Id() == 1 && process.State() == ProcessState::Created,
               "process creation failed", failure)) {
        report.failure = failure;
        report.log = log.str() + "Runtime smoke tests:\nFAIL\n" + report.failure + "\n";
        return report;
    }
    log << "Process created:\nPID " << process.Id() << "\n\n";

    Scheduler scheduler;
    std::string schedulerError;
    auto* thread = scheduler.CreateThread(
        process, CodeAddress, StackAddress, 0x1000, &schedulerError);
    if (!Check(thread != nullptr, "thread creation failed", failure) ||
        !Check(thread->Id() == 1, "unexpected thread identifier", failure) ||
        !Check(thread->Cpu().instructionPointer == CodeAddress,
               "thread CPU entry point was not initialized", failure) ||
        !Check(thread->Stack().topAddress == StackAddress + 0x1000,
               "thread stack information was not initialized", failure)) {
        report.failure = failure + (schedulerError.empty() ? "" : ": " + schedulerError);
        report.log = log.str() + "Runtime smoke tests:\nFAIL\n" + report.failure + "\n";
        return report;
    }
    log << "Thread created:\nTID " << thread->Id() << "\n\n";

    const auto execution = scheduler.Run(process, 16);
    log << execution.log << '\n';
    if (!Check(execution.success, "scheduler did not complete the synthetic thread", failure) ||
        !Check(thread->State() == ThreadState::Halted,
               "scheduler did not preserve the halted thread state", failure) ||
        !Check(process.State() == ProcessState::Ready,
               "scheduler did not return the process to Ready", failure)) {
        report.failure = failure;
    }

    SyscallDispatcher syscalls;
    std::string syscallError;
    if (!syscalls.Register(
            0x42,
            "RuntimeTest",
            [](SyscallContext&) {
                SyscallResult result;
                result.success = true;
                result.value = 0x42;
                return result;
            },
            &syscallError)) {
        report.failure = "syscall registration failed" +
            (syscallError.empty() ? std::string{} : ": " + syscallError);
    }

    SyscallContext context{process, *thread, thread->Cpu()};
    const auto customSyscall = syscalls.Dispatch(0x42, context);
    log << customSyscall.log << '\n';
    if (failure.empty() &&
        !Check(customSyscall.success, "registered syscall did not dispatch", failure)) {
        report.failure = failure;
    }

    DefaultExceptionHandler exceptionHandler;
    ExceptionBoundary exceptionBoundary(exceptionHandler);
    std::string transitionLog;
    const auto transition = exceptionBoundary.HandleSyscallTransition(
        process, *thread, static_cast<std::uint64_t>(Syscall::ProcessExit), &transitionLog);
    log << transitionLog << '\n';
    if (failure.empty() &&
        !Check(transition == ExceptionDisposition::Continue,
               "syscall transition was not accepted by the exception boundary", failure)) {
        report.failure = failure;
    }

    const auto exit = syscalls.Dispatch(Syscall::ProcessExit, context);
    log << exit.log << '\n';
    if (failure.empty() &&
        (!Check(exit.success, "ProcessExit syscall failed", failure) ||
         !Check(thread->State() == ThreadState::Exited,
                "ProcessExit did not exit the thread", failure) ||
         !Check(process.State() == ProcessState::Terminated,
                "ProcessExit did not terminate the process", failure))) {
        report.failure = failure;
    }

    Memory::GuestMemory invalidMemory;
    const std::uint8_t invalidOpcode = 0xFE;
    const Memory::MemoryRegion invalidRegion{
        InvalidAddress,
        1,
        Permissions(Memory::MemoryPermission::Read, Memory::MemoryPermission::Execute),
        "runtime invalid instruction",
    };
    if (!invalidMemory.Map(invalidRegion, &invalidOpcode, sizeof(invalidOpcode), &memoryError)) {
        if (report.failure.empty()) {
            report.failure = "unable to map invalid runtime instruction: " + memoryError;
        }
    } else {
        Process faultedProcess(2, invalidMemory);
        Scheduler faultScheduler;
        auto* faultedThread = faultScheduler.CreateThread(
            faultedProcess, InvalidAddress, StackAddress, 0x1000, &schedulerError);
        const auto faultExecution = faultScheduler.Run(faultedProcess, 4);
        log << "Exception boundary test\n" << faultExecution.log << '\n';
        if (report.failure.empty() &&
            (!Check(faultedThread != nullptr, "fault test thread creation failed", failure) ||
             !Check(!faultExecution.success, "invalid instruction unexpectedly succeeded", failure) ||
             !Check(faultedProcess.State() == ProcessState::Faulted,
                    "CPU fault did not reach the process boundary", failure) ||
             !Check(faultedThread->State() == ThreadState::Faulted,
                    "CPU fault did not fault the thread", failure))) {
            report.failure = failure;
        }
    }

    if (report.failure.empty()) {
        report.passed = true;
        log << "Process terminated successfully\n\nRuntime smoke tests:\nPASS\n";
    } else {
        log << "Runtime smoke tests:\nFAIL\n" << report.failure << '\n';
    }
    report.log = log.str();
    return report;
}

} // namespace ChonkyStation4::Core::Kernel
