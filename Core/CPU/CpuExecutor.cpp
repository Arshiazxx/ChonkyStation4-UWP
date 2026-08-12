#include "CpuExecutor.hpp"

#include <iomanip>
#include <limits>
#include <sstream>

namespace ChonkyStation4::Core::CPU {

namespace {

std::string Hex(std::uint64_t value) {
    std::ostringstream text;
    text << "0x" << std::uppercase << std::hex << value;
    return text.str();
}

bool AddSigned(
    std::uint64_t base,
    std::int32_t displacement,
    std::uint64_t& result) noexcept {
    if (displacement >= 0) {
        const auto offset = static_cast<std::uint64_t>(displacement);
        if (offset > (std::numeric_limits<std::uint64_t>::max)() - base) {
            return false;
        }
        result = base + offset;
        return true;
    }

    const auto magnitude = static_cast<std::uint64_t>(-
        static_cast<std::int64_t>(displacement));
    if (magnitude > base) {
        return false;
    }
    result = base - magnitude;
    return true;
}

void AppendLine(std::ostringstream& log, const std::string& line) {
    log << line << '\n';
}

CpuExceptionKind ClassifyError(const std::string& error) {
    if (error.find("invalid synthetic opcode") != std::string::npos) {
        return CpuExceptionKind::InvalidInstruction;
    }
    if (error.find("instruction fetch") != std::string::npos ||
        error.find("guest memory") != std::string::npos ||
        error.find("LOAD") != std::string::npos ||
        error.find("STORE") != std::string::npos) {
        return CpuExceptionKind::MemoryFault;
    }
    if (error.find("address") != std::string::npos ||
        error.find("overflow") != std::string::npos) {
        return CpuExceptionKind::AddressFault;
    }
    return CpuExceptionKind::ExecutionFault;
}

} // namespace

CpuExecutor::CpuExecutor(Memory::GuestMemory& memory) noexcept
    : memory_(memory) {}

FetchedInstruction CpuExecutor::Fetch(const CpuState& state) const {
    FetchedInstruction result;
    std::string error;
    std::uint8_t opcode = 0;
    if (!memory_.Read(state.instructionPointer, &opcode, sizeof(opcode), &error)) {
        result.error = "instruction fetch failed at " + Hex(state.instructionPointer) + ": " + error;
        return result;
    }

    const auto length = InstructionDecoder::EncodedLength(opcode);
    if (length > result.bytes.size()) {
        result.error = "decoded instruction exceeds fetch buffer";
        return result;
    }
    if (!memory_.Read(state.instructionPointer, result.bytes.data(), length, &error)) {
        result.error = "instruction fetch failed at " + Hex(state.instructionPointer) + ": " + error;
        return result;
    }

    const auto decoded = decoder_.Decode(result.bytes.data(), length);
    if (!decoded.success) {
        result.error = decoded.error;
        return result;
    }
    result.instruction = decoded.instruction;
    result.success = true;
    return result;
}

CpuStepReport CpuExecutor::Step(CpuState& state) const {
    CpuStepReport result;
    if (state.executionState == ExecutionState::Halted) {
        result.result = StepResult::Halted;
        result.error = "CPU is already halted";
        return result;
    }
    if (state.executionState == ExecutionState::Faulted ||
        state.executionState == ExecutionState::StepLimitReached) {
        result.error = state.exceptionMessage;
        return result;
    }
    state.executionState = ExecutionState::Running;

    const auto rip = state.instructionPointer;
    const auto fetched = Fetch(state);
    if (!fetched.success) {
        state.executionState = ExecutionState::Faulted;
        state.exceptionKind = ClassifyError(fetched.error);
        state.exceptionMessage = fetched.error;
        result.error = fetched.error;
        std::ostringstream log;
        AppendLine(log, "RIP: " + Hex(rip));
        AppendLine(log, "Exception:");
        AppendLine(log, fetched.error);
        result.log = log.str();
        return result;
    }

    result.instruction = fetched.instruction;
    std::ostringstream log;
    AppendLine(log, "RIP: " + Hex(rip));
    AppendLine(log, "Instruction:");
    AppendLine(log, InstructionDecoder::Format(fetched.instruction));

    if (fetched.instruction.encodedSize >
        (std::numeric_limits<std::uint64_t>::max)() - rip) {
        state.executionState = ExecutionState::Faulted;
        state.exceptionKind = CpuExceptionKind::AddressFault;
        state.exceptionMessage = "instruction pointer overflow";
        result.error = state.exceptionMessage;
        AppendLine(log, "Exception:");
        AppendLine(log, result.error);
        result.log = log.str();
        return result;
    }
    const auto nextInstructionPointer = rip + fetched.instruction.encodedSize;

    switch (fetched.instruction.opcode) {
    case Opcode::Nop:
        state.instructionPointer = nextInstructionPointer;
        state.executedInstructions++;
        result.result = StepResult::Executed;
        AppendLine(log, "Execution: NOP");
        break;

    case Opcode::MovImmediate:
        state.registers[fetched.instruction.destination] = fetched.instruction.immediate;
        state.instructionPointer = nextInstructionPointer;
        state.executedInstructions++;
        result.result = StepResult::Executed;
        AppendLine(log, std::string(Registers::Name(fetched.instruction.destination)) +
            " = " + std::to_string(fetched.instruction.immediate));
        break;

    case Opcode::AddImmediate: {
        const auto left = state.registers[fetched.instruction.destination];
        const auto right = fetched.instruction.immediate;
        const auto value = left + right;
        state.registers[fetched.instruction.destination] = value;
        state.flags.UpdateAdd(left, right, value);
        state.instructionPointer = nextInstructionPointer;
        state.executedInstructions++;
        result.result = StepResult::Executed;
        AppendLine(log, std::string(Registers::Name(fetched.instruction.destination)) +
            " = " + std::to_string(value));
        break;
    }

    case Opcode::SubImmediate: {
        const auto left = state.registers[fetched.instruction.destination];
        const auto right = fetched.instruction.immediate;
        const auto value = left - right;
        state.registers[fetched.instruction.destination] = value;
        state.flags.UpdateSub(left, right, value);
        state.instructionPointer = nextInstructionPointer;
        state.executedInstructions++;
        result.result = StepResult::Executed;
        AppendLine(log, std::string(Registers::Name(fetched.instruction.destination)) +
            " = " + std::to_string(value));
        break;
    }

    case Opcode::Load: {
        std::uint64_t address = 0;
        if (!EffectiveAddress(state, fetched.instruction, address, result.error)) {
            state.executionState = ExecutionState::Faulted;
            state.exceptionKind = CpuExceptionKind::AddressFault;
            state.exceptionMessage = result.error;
            AppendLine(log, "Exception:");
            AppendLine(log, result.error);
            result.log = log.str();
            return result;
        }
        std::uint64_t value = 0;
        if (!memory_.Read(address, &value, sizeof(value), &result.error)) {
            state.executionState = ExecutionState::Faulted;
            state.exceptionKind = CpuExceptionKind::MemoryFault;
            state.exceptionMessage = "LOAD at " + Hex(address) + " failed: " + result.error;
            result.error = state.exceptionMessage;
            AppendLine(log, "Exception:");
            AppendLine(log, result.error);
            result.log = log.str();
            return result;
        }
        state.registers[fetched.instruction.destination] = value;
        state.instructionPointer = nextInstructionPointer;
        state.executedInstructions++;
        result.result = StepResult::Executed;
        AppendLine(log, std::string(Registers::Name(fetched.instruction.destination)) +
            " = " + std::to_string(value));
        break;
    }

    case Opcode::Store: {
        std::uint64_t address = 0;
        if (!EffectiveAddress(state, fetched.instruction, address, result.error)) {
            state.executionState = ExecutionState::Faulted;
            state.exceptionKind = CpuExceptionKind::AddressFault;
            state.exceptionMessage = result.error;
            AppendLine(log, "Exception:");
            AppendLine(log, result.error);
            result.log = log.str();
            return result;
        }
        const auto value = state.registers[fetched.instruction.destination];
        if (!memory_.Write(address, &value, sizeof(value), &result.error)) {
            state.executionState = ExecutionState::Faulted;
            state.exceptionKind = CpuExceptionKind::MemoryFault;
            state.exceptionMessage = "STORE at " + Hex(address) + " failed: " + result.error;
            result.error = state.exceptionMessage;
            AppendLine(log, "Exception:");
            AppendLine(log, result.error);
            result.log = log.str();
            return result;
        }
        state.instructionPointer = nextInstructionPointer;
        state.executedInstructions++;
        result.result = StepResult::Executed;
        AppendLine(log, "Memory[" + Hex(address) + "] = " + std::to_string(value));
        break;
    }

    case Opcode::Halt:
        state.instructionPointer = nextInstructionPointer;
        state.executedInstructions++;
        state.executionState = ExecutionState::Halted;
        result.result = StepResult::Halted;
        AppendLine(log, "Execution: HALT");
        break;
    }

    result.log = log.str();
    return result;
}

CpuExecutionReport CpuExecutor::Run(CpuState& state, std::uint64_t maxInstructions) const {
    CpuExecutionReport report;
    std::ostringstream log;
    AppendLine(log, "ChonkyStation4 CPU");
    AppendLine(log, "");
    AppendLine(log, "Starting execution");
    AppendLine(log, "");

    if (state.executionState == ExecutionState::Halted ||
        state.executionState == ExecutionState::Faulted) {
        state.Reset();
    }
    state.executionState = ExecutionState::Running;
    state.exceptionKind = CpuExceptionKind::None;
    state.exceptionMessage.clear();

    std::uint64_t steps = 0;
    while (steps < maxInstructions) {
        const auto step = Step(state);
        log << step.log;
        ++steps;
        if (step.result == StepResult::Halted || step.result == StepResult::Faulted) {
            if (step.result == StepResult::Faulted) {
                report.error = step.error;
            }
            break;
        }
    }

    if (state.executionState == ExecutionState::Running) {
        state.executionState = ExecutionState::StepLimitReached;
        state.exceptionKind = CpuExceptionKind::StepLimit;
        state.exceptionMessage = "instruction limit reached";
        report.error = state.exceptionMessage;
        AppendLine(log, "Exception:");
        AppendLine(log, report.error);
    }

    AppendLine(log, "");
    AppendLine(log, "Execution:");
    switch (state.executionState) {
    case ExecutionState::Halted:
        AppendLine(log, "HALTED");
        break;
    case ExecutionState::Faulted:
        AppendLine(log, "FAULTED");
        break;
    case ExecutionState::StepLimitReached:
        AppendLine(log, "STEP LIMIT REACHED");
        break;
    case ExecutionState::Ready:
    case ExecutionState::Running:
        AppendLine(log, "STOPPED");
        break;
    }

    report.success = state.executionState == ExecutionState::Halted;
    report.finalState = state.executionState;
    report.exceptionKind = state.exceptionKind;
    report.instructionsExecuted = state.executedInstructions;
    report.log = log.str();
    return report;
}

bool CpuExecutor::EffectiveAddress(
    const CpuState& state,
    const Instruction& instruction,
    std::uint64_t& address,
    std::string& error) const {
    const auto base = state.registers[instruction.source];
    if (!AddSigned(base, instruction.displacement, address)) {
        error = "effective address overflows guest address space";
        return false;
    }
    return true;
}

} // namespace ChonkyStation4::Core::CPU
