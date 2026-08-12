#pragma once

#include "Core/ABI/Arguments/AbiContext.hpp"
#include "Core/CPU/CpuState.hpp"
#include "Core/Kernel/Process/Process.hpp"
#include "Core/Kernel/Thread/Thread.hpp"

#include <cstdint>
#include <string>

namespace ChonkyStation4::Core::Execution {

struct ExecutionExceptionState {
    CPU::CpuExceptionKind kind = CPU::CpuExceptionKind::None;
    std::string message;

    bool Pending() const noexcept {
        return kind != CPU::CpuExceptionKind::None;
    }
};

// A backend-neutral snapshot of the state needed to cross from the kernel
// scheduler into an execution implementation. It stores state rather than
// invoking host code, so native/JIT/synthetic backends can be added later.
class ExecutionContext final {
public:
    ExecutionContext(Kernel::Process& process, Kernel::Thread& thread) noexcept;

    Kernel::Process& ProcessReference() noexcept;
    const Kernel::Process& ProcessReference() const noexcept;
    Kernel::Thread& ThreadReference() noexcept;
    const Kernel::Thread& ThreadReference() const noexcept;

    CPU::CpuState& CpuState() noexcept;
    const CPU::CpuState& CpuState() const noexcept;
    CPU::Registers& GeneralRegisters() noexcept;
    const CPU::Registers& GeneralRegisters() const noexcept;

    std::uint64_t InstructionPointer() const noexcept;
    void SetInstructionPointer(std::uint64_t value) noexcept;
    std::uint64_t StackPointer() const noexcept;
    void SetStackPointer(std::uint64_t value) noexcept;

    ABI::AbiContext AbiState() noexcept;
    ExecutionExceptionState ExceptionState() const;
    bool IsValid() const noexcept;

    void CaptureFromThread() noexcept;
    void ApplyToThread() noexcept;

private:
    Kernel::Process* process_ = nullptr;
    Kernel::Thread* thread_ = nullptr;
    CPU::CpuState cpu_{};
};

} // namespace ChonkyStation4::Core::Execution
