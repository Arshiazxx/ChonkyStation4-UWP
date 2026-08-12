#include "ExecutionContext.hpp"

namespace ChonkyStation4::Core::Execution {

ExecutionContext::ExecutionContext(
    Kernel::Process& process,
    Kernel::Thread& thread) noexcept
    : process_(&process), thread_(&thread), cpu_(thread.Cpu()) {}

Kernel::Process& ExecutionContext::ProcessReference() noexcept {
    return *process_;
}

const Kernel::Process& ExecutionContext::ProcessReference() const noexcept {
    return *process_;
}

Kernel::Thread& ExecutionContext::ThreadReference() noexcept {
    return *thread_;
}

const Kernel::Thread& ExecutionContext::ThreadReference() const noexcept {
    return *thread_;
}

CPU::CpuState& ExecutionContext::CpuState() noexcept {
    return cpu_;
}

const CPU::CpuState& ExecutionContext::CpuState() const noexcept {
    return cpu_;
}

CPU::Registers& ExecutionContext::GeneralRegisters() noexcept {
    return cpu_.registers;
}

const CPU::Registers& ExecutionContext::GeneralRegisters() const noexcept {
    return cpu_.registers;
}

std::uint64_t ExecutionContext::InstructionPointer() const noexcept {
    return cpu_.instructionPointer;
}

void ExecutionContext::SetInstructionPointer(std::uint64_t value) noexcept {
    cpu_.instructionPointer = value;
}

std::uintptr_t ExecutionContext::NativeEntryPoint() const noexcept {
    return nativeEntryPoint_;
}

void ExecutionContext::SetNativeEntryPoint(std::uintptr_t value) noexcept {
    nativeEntryPoint_ = value;
}

std::uint64_t ExecutionContext::StackPointer() const noexcept {
    return cpu_.StackPointer();
}

void ExecutionContext::SetStackPointer(std::uint64_t value) noexcept {
    cpu_.SetStackPointer(value);
}

ABI::AbiContext ExecutionContext::AbiState() noexcept {
    return ABI::AbiContext(cpu_);
}

ExecutionExceptionState ExecutionContext::ExceptionState() const {
    return {cpu_.exceptionKind, cpu_.exceptionMessage};
}

bool ExecutionContext::IsValid() const noexcept {
    return process_ != nullptr && thread_ != nullptr &&
        &thread_->OwnerProcess() == process_ &&
        cpu_.instructionPointer != InvalidGuestVirtualAddress;
}

void ExecutionContext::CaptureFromThread() noexcept {
    if (thread_ != nullptr) {
        cpu_ = thread_->Cpu();
    }
}

void ExecutionContext::ApplyToThread() noexcept {
    if (thread_ != nullptr) {
        thread_->Cpu() = cpu_;
    }
}

} // namespace ChonkyStation4::Core::Execution
