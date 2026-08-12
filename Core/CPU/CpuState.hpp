#pragma once

#include "Core/CPU/Flags.hpp"
#include "Core/CPU/Registers.hpp"

#include <cstdint>
#include <string>

namespace ChonkyStation4::Core::CPU {

enum class ExecutionState {
    Ready,
    Running,
    Halted,
    Faulted,
    StepLimitReached,
};

enum class CpuExceptionKind {
    None,
    MemoryFault,
    InvalidInstruction,
    AddressFault,
    ExecutionFault,
    StepLimit,
};

struct CpuState {
    Registers registers;
    std::uint64_t instructionPointer = 0;
    Flags flags;
    ExecutionState executionState = ExecutionState::Ready;
    CpuExceptionKind exceptionKind = CpuExceptionKind::None;
    std::uint64_t executedInstructions = 0;
    std::string exceptionMessage;

    std::uint64_t StackPointer() const noexcept {
        return registers[RegisterId::Rsp];
    }

    void SetStackPointer(std::uint64_t value) noexcept {
        registers[RegisterId::Rsp] = value;
    }

    void Reset() noexcept {
        registers.Reset();
        instructionPointer = 0;
        flags.Reset();
        executionState = ExecutionState::Ready;
        exceptionKind = CpuExceptionKind::None;
        executedInstructions = 0;
        exceptionMessage.clear();
    }
};

} // namespace ChonkyStation4::Core::CPU
