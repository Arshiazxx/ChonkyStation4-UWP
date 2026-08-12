#include "Thread.hpp"

#include <limits>

namespace ChonkyStation4::Core::Kernel {

Thread::Thread(
    ThreadId id,
    Process& process,
    GuestVirtualAddress entryPoint,
    GuestVirtualAddress stackBase,
    std::uint64_t stackSize) noexcept
    : id_(id), process_(&process), stack_{stackBase, stackSize, stackBase} {
    cpu_.Reset();
    cpu_.instructionPointer = entryPoint;
    if (stackSize <= (std::numeric_limits<GuestVirtualAddress>::max)() - stackBase) {
        stack_.topAddress = stackBase + stackSize;
        cpu_.SetStackPointer(stack_.topAddress);
    }
    state_ = ThreadState::Ready;
}

ThreadId Thread::Id() const noexcept {
    return id_;
}

Process& Thread::OwnerProcess() noexcept {
    return *process_;
}

const Process& Thread::OwnerProcess() const noexcept {
    return *process_;
}

ThreadState Thread::State() const noexcept {
    return state_;
}

void Thread::SetState(ThreadState state) noexcept {
    state_ = state;
}

CPU::CpuState& Thread::Cpu() noexcept {
    return cpu_;
}

const CPU::CpuState& Thread::Cpu() const noexcept {
    return cpu_;
}

const StackInfo& Thread::Stack() const noexcept {
    return stack_;
}

void Thread::MarkRunning() noexcept {
    state_ = ThreadState::Running;
    cpu_.executionState = CPU::ExecutionState::Running;
}

void Thread::MarkHalted() noexcept {
    state_ = ThreadState::Halted;
}

void Thread::MarkExited(std::int64_t exitCode) noexcept {
    exitCode_ = exitCode;
    state_ = ThreadState::Exited;
    exceptionMessage_.clear();
}

void Thread::MarkFaulted(const std::string& message) {
    state_ = ThreadState::Faulted;
    exceptionMessage_ = message;
}

std::int64_t Thread::ExitCode() const noexcept {
    return exitCode_;
}

const std::string& Thread::ExceptionMessage() const noexcept {
    return exceptionMessage_;
}

} // namespace ChonkyStation4::Core::Kernel
