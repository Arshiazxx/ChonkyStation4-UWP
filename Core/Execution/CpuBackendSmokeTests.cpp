#include "CpuBackendSmokeTests.hpp"

#include "Core/CPU/Instruction.hpp"
#include "Core/Execution/ExecutionContext.hpp"
#include "Core/Execution/SyntheticTestBackend.hpp"
#include "Core/Execution/UpstreamCompatibleX64Backend.hpp"
#include "Core/Kernel/Process/Process.hpp"
#include "Core/Kernel/Thread/Thread.hpp"
#include "Core/Memory/GuestMemory.hpp"

#include <cstdint>
#include <sstream>
#include <vector>

namespace ChonkyStation4::Core::Execution {

namespace {

constexpr std::uint64_t CodeAddress = 0x400000;
constexpr std::uint64_t StackAddress = 0x600000;

void AppendU64(std::vector<std::uint8_t>& bytes, std::uint64_t value) {
    for (std::size_t index = 0; index < sizeof(value); ++index) {
        bytes.push_back(static_cast<std::uint8_t>(value >> (index * 8)));
    }
}

std::vector<std::uint8_t> MakeSyntheticProgram() {
    std::vector<std::uint8_t> bytes;
    bytes.push_back(static_cast<std::uint8_t>(CPU::Opcode::MovImmediate));
    bytes.push_back(static_cast<std::uint8_t>(CPU::RegisterId::Rax));
    AppendU64(bytes, 42);
    bytes.push_back(static_cast<std::uint8_t>(CPU::Opcode::Halt));
    return bytes;
}

void Fail(CpuBackendSmokeTestReport& report, const char* message) {
    if (report.failure.empty()) {
        report.failure = message;
    }
}

} // namespace

CpuBackendSmokeTestReport RunCpuBackendSmokeTests() {
    CpuBackendSmokeTestReport report;
    Memory::GuestMemory memory;
    const auto program = MakeSyntheticProgram();
    const Memory::MemoryRegion codeRegion{
        CodeAddress,
        0x1000,
        Memory::ToPermissions(Memory::MemoryPermission::Read) |
            Memory::ToPermissions(Memory::MemoryPermission::Execute),
        "CPU backend synthetic program",
    };
    std::string memoryError;
    if (!memory.Map(codeRegion, program.data(), program.size(), &memoryError)) {
        report.failure = "unable to map backend validation program: " + memoryError;
    }

    Kernel::Process process(1, memory);
    Kernel::Thread mainThread(1, process, CodeAddress, StackAddress, 0x10000);
    process.AttachThread(mainThread.Id());
    ExecutionContext context(process, mainThread);

    SyntheticTestBackend syntheticBackend;
    const auto synthetic = syntheticBackend.Start(context);
    if (report.failure.empty() &&
        (!synthetic.available || !synthetic.accepted || !synthetic.executed ||
         mainThread.Cpu().registers[CPU::RegisterId::Rax] != 42)) {
        Fail(report, "synthetic backend did not complete the M8 validation program");
    }

    UpstreamCompatibleX64Backend upstreamBackend;
    const auto upstream = upstreamBackend.Start(context);
    if (report.failure.empty() &&
        (!upstream.available || !upstream.accepted || upstream.executed)) {
        Fail(report, "upstream-compatible x86-64 boundary was not available safely");
    }

    std::ostringstream output;
    output << "CPU Backend Test\n\n"
           << "Synthetic backend:\n"
           << (synthetic.available && synthetic.accepted && synthetic.executed
                   ? "PASS"
                   : "FAIL") << "\n\n"
           << "Upstream compatible backend:\n"
           << (upstream.available ? "AVAILABLE" : "UNAVAILABLE") << "\n\n"
           << "Architecture:\nx86-64\n\n"
           << "Result:\n"
           << (report.failure.empty() ? "SUCCESS" : "FAILURE") << "\n";
    if (!report.failure.empty()) {
        output << "\nError:\n" << report.failure << "\n";
    }
    report.log = output.str();
    report.passed = report.failure.empty();
    return report;
}

} // namespace ChonkyStation4::Core::Execution
