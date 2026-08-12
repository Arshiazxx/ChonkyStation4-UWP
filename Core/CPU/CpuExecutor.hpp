#pragma once

#include "Core/CPU/CpuState.hpp"
#include "Core/CPU/Instruction.hpp"
#include "Core/Memory/GuestMemory.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

namespace ChonkyStation4::Core::CPU {

struct FetchedInstruction {
    bool success = false;
    std::array<std::uint8_t, Instruction::MaxEncodedSize> bytes{};
    Instruction instruction{};
    std::string error;
};

enum class StepResult {
    Executed,
    Halted,
    Faulted,
};

struct CpuStepReport {
    StepResult result = StepResult::Faulted;
    Instruction instruction{};
    std::string log;
    std::string error;
};

struct CpuExecutionReport {
    bool success = false;
    ExecutionState finalState = ExecutionState::Ready;
    CpuExceptionKind exceptionKind = CpuExceptionKind::None;
    std::uint64_t instructionsExecuted = 0;
    std::string log;
    std::string error;

    std::string ToText() const { return log; }
};

class CpuExecutor final {
public:
    explicit CpuExecutor(Memory::GuestMemory& memory) noexcept;

    FetchedInstruction Fetch(const CpuState& state) const;
    CpuStepReport Step(CpuState& state) const;
    CpuExecutionReport Run(CpuState& state, std::uint64_t maxInstructions = 10000) const;

private:
    bool EffectiveAddress(
        const CpuState& state,
        const Instruction& instruction,
        std::uint64_t& address,
        std::string& error) const;

    Memory::GuestMemory& memory_;
    InstructionDecoder decoder_;
};

} // namespace ChonkyStation4::Core::CPU
