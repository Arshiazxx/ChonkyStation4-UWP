#include "CpuSmokeTests.hpp"

#include "CpuExecutor.hpp"

#include <cstdint>
#include <sstream>
#include <vector>

namespace ChonkyStation4::Core::CPU {

namespace {

constexpr std::uint64_t CodeAddress = 0x400000;
constexpr std::uint64_t DataAddress = 0x500000;

std::uint32_t Permissions(Memory::MemoryPermission first, Memory::MemoryPermission second) {
    return Memory::ToPermissions(first) | Memory::ToPermissions(second);
}

void AppendU32(std::vector<std::uint8_t>& program, std::uint32_t value) {
    for (std::size_t index = 0; index < 4; ++index) {
        program.push_back(static_cast<std::uint8_t>(value >> (index * 8)));
    }
}

void AppendU64(std::vector<std::uint8_t>& program, std::uint64_t value) {
    for (std::size_t index = 0; index < 8; ++index) {
        program.push_back(static_cast<std::uint8_t>(value >> (index * 8)));
    }
}

void AppendImmediate(
    std::vector<std::uint8_t>& program,
    Opcode opcode,
    RegisterId destination,
    std::uint64_t value) {
    program.push_back(static_cast<std::uint8_t>(opcode));
    program.push_back(static_cast<std::uint8_t>(destination));
    AppendU64(program, value);
}

void AppendMemory(
    std::vector<std::uint8_t>& program,
    Opcode opcode,
    RegisterId valueRegister,
    RegisterId baseRegister,
    std::int32_t displacement) {
    program.push_back(static_cast<std::uint8_t>(opcode));
    program.push_back(static_cast<std::uint8_t>(valueRegister));
    program.push_back(static_cast<std::uint8_t>(baseRegister));
    AppendU32(program, static_cast<std::uint32_t>(displacement));
}

bool Check(bool condition, const char* failure, std::string& error) {
    if (condition) {
        return true;
    }
    error = failure;
    return false;
}

} // namespace

CpuSmokeTestReport RunCpuSmokeTests() {
    CpuSmokeTestReport report;
    std::ostringstream log;
    std::string failure;

    CpuState initial;
    if (!Check(initial.registers[RegisterId::Rax] == 0,
               "CPU initialization did not clear RAX", failure) ||
        !Check(initial.instructionPointer == 0,
               "CPU initialization did not clear RIP", failure) ||
        !Check(initial.StackPointer() == 0,
               "CPU initialization did not clear RSP", failure) ||
        !Check(initial.flags.Raw() == 0,
               "CPU initialization did not clear flags", failure) ||
        !Check(initial.executionState == ExecutionState::Ready,
               "CPU initialization did not set Ready state", failure)) {
        report.failure = failure;
        report.log = "ChonkyStation4 CPU\n\nCPU smoke tests:\nFAIL\n" + failure + "\n";
        return report;
    }

    std::vector<std::uint8_t> program;
    AppendImmediate(program, Opcode::MovImmediate, RegisterId::Rax, 10);
    AppendImmediate(program, Opcode::AddImmediate, RegisterId::Rax, 5);
    AppendImmediate(program, Opcode::MovImmediate, RegisterId::Rbx, DataAddress);
    AppendMemory(program, Opcode::Store, RegisterId::Rax, RegisterId::Rbx, 0);
    AppendMemory(program, Opcode::Load, RegisterId::Rcx, RegisterId::Rbx, 0);
    AppendImmediate(program, Opcode::SubImmediate, RegisterId::Rcx, 3);
    program.push_back(static_cast<std::uint8_t>(Opcode::Halt));

    Memory::GuestMemory memory;
    std::string memoryError;
    const Memory::MemoryRegion codeRegion{
        CodeAddress,
        0x1000,
        Permissions(Memory::MemoryPermission::Read, Memory::MemoryPermission::Execute),
        "CPU synthetic program",
    };
    const Memory::MemoryRegion dataRegion{
        DataAddress,
        0x1000,
        Permissions(Memory::MemoryPermission::Read, Memory::MemoryPermission::Write),
        "CPU synthetic data",
    };
    if (!Check(memory.Map(codeRegion, program.data(), program.size(), &memoryError),
               "unable to map synthetic CPU program", failure) ||
        !Check(memory.Map(dataRegion, nullptr, 0, &memoryError),
               "unable to map synthetic CPU data", failure)) {
        report.failure = failure + ": " + memoryError;
        report.log = "ChonkyStation4 CPU\n\nCPU smoke tests:\nFAIL\n" + report.failure + "\n";
        return report;
    }

    CpuState state;
    state.instructionPointer = CodeAddress;
    state.SetStackPointer(0x600000);
    CpuExecutor executor(memory);
    const auto execution = executor.Run(state);
    log << execution.log << '\n';

    if (!Check(execution.success, "synthetic program did not halt", failure) ||
        !Check(state.registers[RegisterId::Rax] == 15,
               "arithmetic execution produced the wrong RAX", failure) ||
        !Check(state.registers[RegisterId::Rcx] == 12,
               "arithmetic execution produced the wrong RCX", failure) ||
        !Check(state.executedInstructions == 7,
               "unexpected synthetic instruction count", failure) ||
        !Check(!state.flags.IsSet(Flag::Zero),
               "SUB incorrectly left the zero flag set", failure)) {
        report.failure = failure;
    }

    const std::uint32_t marker = 0xC0DEFACE;
    std::uint32_t markerReadBack = 0;
    if (failure.empty() &&
        (!Check(memory.Write(DataAddress + 0x20, &marker, sizeof(marker), &memoryError),
                "guest memory write failed", failure) ||
         !Check(memory.Read(DataAddress + 0x20, &markerReadBack, sizeof(markerReadBack), &memoryError),
                "guest memory read failed", failure) ||
         !Check(markerReadBack == marker, "guest memory read/write mismatch", failure))) {
        report.failure = failure + (memoryError.empty() ? "" : ": " + memoryError);
    }

    Memory::GuestMemory invalidMemory;
    const std::uint8_t invalidOpcode = 0xFE;
    const Memory::MemoryRegion invalidRegion{
        0x700000,
        1,
        Permissions(Memory::MemoryPermission::Read, Memory::MemoryPermission::Execute),
        "invalid CPU instruction",
    };
    if (!invalidMemory.Map(invalidRegion, &invalidOpcode, sizeof(invalidOpcode), &memoryError)) {
        if (failure.empty()) {
            report.failure = "unable to map invalid instruction test" +
                (memoryError.empty() ? std::string{} : ": " + memoryError);
        }
    } else {
        CpuState invalidState;
        invalidState.instructionPointer = 0x700000;
        CpuExecutor invalidExecutor(invalidMemory);
        const auto invalidExecution = invalidExecutor.Run(invalidState, 4);
        log << "Invalid instruction test\n" << invalidExecution.log << '\n';
        if (failure.empty() &&
            !Check(invalidExecution.finalState == ExecutionState::Faulted,
                   "invalid instruction did not fault the CPU", failure)) {
            report.failure = failure;
        }
    }

    if (report.failure.empty()) {
        report.passed = true;
        log << "CPU smoke tests:\nPASS\n";
    } else {
        log << "CPU smoke tests:\nFAIL\n" << report.failure << '\n';
    }
    report.log = log.str();
    return report;
}

} // namespace ChonkyStation4::Core::CPU
