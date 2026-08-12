#include "ModuleSmokeTests.hpp"

#include "Core/Execution/ExecutionContext.hpp"
#include "Core/Execution/NativeExecutionBackend.hpp"
#include "Core/Kernel/Process/Process.hpp"
#include "Core/Kernel/Thread/Thread.hpp"
#include "Core/Loader/Module/LoadedModule.hpp"
#include "Core/Loader/Relocations/Relocation.hpp"
#include "Core/Loader/Elf64Loader.hpp"
#include "Core/Memory/GuestMemory.hpp"

#include <array>
#include <cstdint>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

namespace ChonkyStation4::Core::Execution {

namespace {

constexpr std::uint64_t TestImageBase = 0x400000;
constexpr std::uint64_t TestEntryPoint = TestImageBase;

void PutU16(std::vector<std::uint8_t>& bytes, std::size_t offset, std::uint16_t value) {
    bytes[offset] = static_cast<std::uint8_t>(value & 0xff);
    bytes[offset + 1] = static_cast<std::uint8_t>((value >> 8) & 0xff);
}

void PutU32(std::vector<std::uint8_t>& bytes, std::size_t offset, std::uint32_t value) {
    for (std::size_t index = 0; index < 4; ++index) {
        bytes[offset + index] = static_cast<std::uint8_t>((value >> (index * 8)) & 0xff);
    }
}

void PutU64(std::vector<std::uint8_t>& bytes, std::size_t offset, std::uint64_t value) {
    for (std::size_t index = 0; index < 8; ++index) {
        bytes[offset + index] = static_cast<std::uint8_t>((value >> (index * 8)) & 0xff);
    }
}

std::vector<std::uint8_t> MakeTestElf() {
    constexpr std::size_t programHeaderOffset = 64;
    constexpr std::size_t segmentFileOffset = 0x1000;
    std::vector<std::uint8_t> bytes(segmentFileOffset + 2, 0);
    bytes[0] = 0x7f;
    bytes[1] = 'E';
    bytes[2] = 'L';
    bytes[3] = 'F';
    bytes[4] = 2; // ELFCLASS64
    bytes[5] = 1; // little endian
    bytes[6] = 1; // current ELF version
    PutU16(bytes, 16, 2); // ET_EXEC
    PutU16(bytes, 18, 62); // EM_X86_64
    PutU32(bytes, 20, 1);
    PutU64(bytes, 24, TestEntryPoint);
    PutU64(bytes, 32, programHeaderOffset);
    PutU32(bytes, 48, 0);
    PutU16(bytes, 52, 64);
    PutU16(bytes, 54, 56);
    PutU16(bytes, 56, 1);

    PutU32(bytes, programHeaderOffset, 1); // PT_LOAD
    PutU32(bytes, programHeaderOffset + 4, 5); // PF_R | PF_X
    PutU64(bytes, programHeaderOffset + 8, segmentFileOffset);
    PutU64(bytes, programHeaderOffset + 16, TestImageBase);
    PutU64(bytes, programHeaderOffset + 24, 0);
    PutU64(bytes, programHeaderOffset + 32, 2);
    PutU64(bytes, programHeaderOffset + 40, 0x1000);
    PutU64(bytes, programHeaderOffset + 48, 0x1000);

    // The bytes are only a load/zero-fill fixture. They are never executed.
    bytes[segmentFileOffset] = 0x90;
    bytes[segmentFileOffset + 1] = 0xc3;
    return bytes;
}

void Fail(ModuleSmokeTestReport& report, const std::string& message) {
    report.failure = message;
}

} // namespace

ModuleSmokeTestReport RunModuleSmokeTests() {
    ModuleSmokeTestReport report;
    Memory::GuestMemory memory;
    Loader::Elf64Loader loader;
    const auto testElf = MakeTestElf();
    Loader::ElfLoadReport elfReport;
    if (!loader.LoadBytesIntoMemory("eboot.bin", testElf, memory, &elfReport)) {
        Fail(report, "test ELF load failed: " + elfReport.error);
    }

    Loader::LoadedModule module("Main Executable", Loader::ModuleKind::MainExecutable);
    if (report.failure.empty() &&
        !Loader::LoadedModule::FromElfReport(
            "Main Executable", elfReport, module, &report.failure)) {
        // FromElfReport supplies the detailed failure.
    }

    Kernel::Process process(1, memory);
    if (report.failure.empty() && !process.RegisterMainModule(module, &report.failure)) {
        // RegisterMainModule supplies the detailed failure.
    }

    GuestVirtualAddress resolvedEntryPoint = InvalidGuestVirtualAddress;
    if (report.failure.empty() &&
        (process.MainModule() == nullptr ||
         !process.MainModule()->ResolveEntryPoint(resolvedEntryPoint))) {
        Fail(report, "main module entry point could not be resolved");
    }
    if (report.failure.empty() && resolvedEntryPoint != TestEntryPoint) {
        Fail(report, "main module entry point does not match the ELF entry point");
    }

    Kernel::Thread mainThread(1, process, resolvedEntryPoint, 0x80000000, 0x10000);
    process.AttachThread(mainThread.Id());
    ExecutionContext context(process, mainThread);
    if (report.failure.empty() && !context.IsValid()) {
        Fail(report, "execution context did not reach a valid backend boundary");
    }
    if (report.failure.empty() && context.InstructionPointer() != resolvedEntryPoint) {
        Fail(report, "execution context instruction pointer is incorrect");
    }

    // Exercise the safety boundary: unsupported and out-of-range records must
    // be reported without touching the loaded image.
    if (report.failure.empty()) {
        Loader::SafeRelocationResolver resolver;
        Loader::RelocationContext relocationContext{
            memory, module.BaseAddress(), module.Size()};
        const auto unsupported = resolver.Resolve(
            {Loader::RelocationType::Unknown, 0, 0, 0, {}}, relocationContext);
        const auto outOfBounds = resolver.Resolve(
            {Loader::RelocationType::Relative64, module.Size(), 0, 0, {}}, relocationContext);
        if (unsupported.status != Loader::RelocationStatus::Unsupported ||
            outOfBounds.status != Loader::RelocationStatus::OutOfBounds) {
            Fail(report, "relocation safety checks did not reject unsafe records");
        }
    }

    NativeExecutionBackend backend;
    const auto boundary = backend.Start(context);
    if (report.failure.empty() &&
        (!boundary.available || !boundary.accepted || boundary.executed)) {
        Fail(report, "execution backend boundary was not reached safely");
    }

    std::ostringstream output;
    output << "ChonkyStation4 Module Test\n\n"
           << "ELF:\neboot.bin\n\n"
           << "Module:\nMain Executable\n\n"
           << "Base:\n0x" << std::uppercase << std::hex
           << (process.MainModule() != nullptr ? process.MainModule()->BaseAddress() : 0)
           << std::dec << "\n\n"
           << "Entry:\n0x" << std::uppercase << std::hex << resolvedEntryPoint
           << std::dec << "\n\n"
           << "Process:\nPID " << process.Id() << "\n\n"
           << "Main Thread:\nTID " << mainThread.Id() << "\n\n"
           << "Execution backend:\n" << (boundary.available ? "Available" : "Unavailable")
           << "\n\n"
           << "Result:\n" << (report.failure.empty() ? "SUCCESS" : "FAILURE") << "\n";
    if (!report.failure.empty()) {
        output << "\nError:\n" << report.failure << "\n";
    }
    report.log = output.str();
    report.passed = report.failure.empty();
    return report;
}

} // namespace ChonkyStation4::Core::Execution
