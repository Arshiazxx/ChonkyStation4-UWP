#include "DynamicModuleSmokeTests.hpp"

#include "Core/Execution/ExecutionContext.hpp"
#include "Core/Execution/NativeExecutionBackend.hpp"
#include "Core/Kernel/Process/Process.hpp"
#include "Core/Kernel/Thread/Thread.hpp"
#include "Core/Loader/Dependencies/DependencyResolver.hpp"
#include "Core/Loader/Module/ModuleRegistry.hpp"
#include "Core/Loader/Relocations/Relocation.hpp"
#include "Core/Loader/Symbols/SymbolResolver.hpp"
#include "Core/Memory/GuestMemory.hpp"

#include <cstdint>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

namespace ChonkyStation4::Core::Execution {

namespace {

constexpr std::uint64_t MainBase = 0x400000;
constexpr std::uint64_t LibraryBase = 0x500000;

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

std::vector<std::uint8_t> MakeTestElf(std::uint64_t base) {
    constexpr std::size_t programHeaderOffset = 64;
    constexpr std::size_t segmentFileOffset = 0x1000;
    std::vector<std::uint8_t> bytes(segmentFileOffset + 8, 0);
    bytes[0] = 0x7f;
    bytes[1] = 'E';
    bytes[2] = 'L';
    bytes[3] = 'F';
    bytes[4] = 2;
    bytes[5] = 1;
    bytes[6] = 1;
    PutU16(bytes, 16, 2); // ET_EXEC
    PutU16(bytes, 18, 62); // EM_X86_64
    PutU32(bytes, 20, 1);
    PutU64(bytes, 24, base);
    PutU64(bytes, 32, programHeaderOffset);
    PutU16(bytes, 52, 64);
    PutU16(bytes, 54, 56);
    PutU16(bytes, 56, 1);
    PutU32(bytes, programHeaderOffset, 1); // PT_LOAD
    PutU32(bytes, programHeaderOffset + 4, 7); // PF_R | PF_W | PF_X
    PutU64(bytes, programHeaderOffset + 8, segmentFileOffset);
    PutU64(bytes, programHeaderOffset + 16, base);
    PutU64(bytes, programHeaderOffset + 32, 8);
    PutU64(bytes, programHeaderOffset + 40, 0x1000);
    PutU64(bytes, programHeaderOffset + 48, 0x1000);
    return bytes;
}

void Fail(DynamicModuleSmokeTestReport& report, const std::string& message) {
    if (report.failure.empty()) {
        report.failure = message;
    }
}

} // namespace

DynamicModuleSmokeTestReport RunDynamicModuleSmokeTests() {
    DynamicModuleSmokeTestReport report;
    Memory::GuestMemory memory;
    Kernel::Process process(1, memory);
    std::string error;

    if (!process.LoadModuleBytes(
            "eboot.bin", "eboot.bin", MakeTestElf(MainBase),
            Loader::ModuleKind::MainExecutable, &error)) {
        Fail(report, "unable to load main module: " + error);
    }
    if (report.failure.empty() && !process.LoadModuleBytes(
            "libkernel.sprx", "libkernel.sprx", MakeTestElf(LibraryBase),
            Loader::ModuleKind::SharedObject, &error)) {
        Fail(report, "unable to load dependency module: " + error);
    }

    auto* mainModule = process.ModuleManager().FindModule("eboot.bin");
    auto* kernelModule = process.ModuleManager().FindModule("libkernel.sprx");
    if (report.failure.empty() && (mainModule == nullptr || kernelModule == nullptr)) {
        Fail(report, "module manager did not register the executable and dependency");
    }

    if (report.failure.empty()) {
        mainModule->AddDependency({"libkernel.sprx", true, false});

        Loader::SymbolInfo exportSymbol;
        exportSymbol.name = "sceKernelCreateThread";
        exportSymbol.value = 0x20;
        exportSymbol.binding = Loader::SymbolBinding::Global;
        exportSymbol.type = Loader::SymbolType::Function;
        exportSymbol.valueKind = Loader::SymbolValueKind::RelativeToModule;
        kernelModule->AddExportedSymbol(exportSymbol);

        Loader::SymbolInfo importSymbol;
        importSymbol.name = "sceKernelCreateThread";
        importSymbol.binding = Loader::SymbolBinding::Global;
        importSymbol.type = Loader::SymbolType::Function;
        mainModule->AddImportedSymbol(importSymbol);

        mainModule->AddRelocation({
            Loader::RelocationType::Absolute64, 0, 0, 0, "sceKernelCreateThread"});
        kernelModule->AddRelocation({
            Loader::RelocationType::Relative64, 0, 0x20, 0, {}});
    }

    const auto dependencyReport = report.failure.empty()
        ? process.ModuleManager().ResolveDependencies("eboot.bin")
        : Loader::DependencyResolutionReport{};
    if (report.failure.empty() && !dependencyReport.success) {
        Fail(report, "dependency resolution failed: " + dependencyReport.error);
    }
    if (report.failure.empty() &&
        (mainModule->Dependencies().empty() || !mainModule->Dependencies()[0].resolved)) {
        Fail(report, "dependency was found but not marked resolved");
    }

    const auto symbolResult = process.ResolveSymbol("sceKernelCreateThread");
    if (report.failure.empty() &&
        (!symbolResult.Found() || symbolResult.moduleName != "libkernel.sprx" ||
         symbolResult.address != LibraryBase + 0x20)) {
        Fail(report, "export symbol lookup returned an unexpected result");
    }
    const auto importReport = report.failure.empty()
        ? process.ModuleManager().ResolveImports("eboot.bin")
        : Loader::ImportResolutionReport{};
    if (report.failure.empty() && (!importReport.success || importReport.resolvedCount != 1)) {
        Fail(report, "import symbol resolution failed");
    }

    const auto libraryRelocations = report.failure.empty()
        ? process.ModuleManager().ApplyRelocations("libkernel.sprx")
        : Loader::RelocationProcessingReport{};
    const auto mainRelocations = report.failure.empty()
        ? process.ModuleManager().ApplyRelocations("eboot.bin")
        : Loader::RelocationProcessingReport{};
    std::uint64_t libraryValue = 0;
    std::uint64_t mainValue = 0;
    if (report.failure.empty() &&
        (!libraryRelocations.success || !mainRelocations.success ||
         libraryRelocations.appliedCount != 1 || mainRelocations.appliedCount != 1 ||
         !memory.Read(LibraryBase, &libraryValue, sizeof(libraryValue), &error) ||
         !memory.Read(MainBase, &mainValue, sizeof(mainValue), &error) ||
         libraryValue != LibraryBase + 0x20 || mainValue != LibraryBase + 0x20)) {
        Fail(report, "relocations were not applied to the expected guest addresses");
    }

        // Verify missing and circular dependencies are reported without involving
    // the process image or mutating guest memory.
    if (report.failure.empty()) {
        Loader::ModuleRegistry missingGraph;
        auto missingRoot = *mainModule;
        missingRoot.AddDependency({"missing.sprx", true, false});
        missingGraph.Register(missingRoot);
        const auto missing = Loader::DependencyResolver(missingGraph).Resolve("eboot.bin");
        if (missing.success || missing.missingDependencies.empty()) {
            Fail(report, "missing dependency was not reported");
        }

        if (!report.failure.empty()) {
            // Keep the circular check below independent of the missing case.
        }
        Loader::ModuleRegistry graph;
        // Rebuild a small cycle from loaded image metadata without mapping a
        // second process image.
        Loader::LoadedModule graphA = *mainModule;
        Loader::LoadedModule graphB = *kernelModule;
        graphA.AddDependency({"libkernel.sprx", true, false});
        graphB.AddDependency({"eboot.bin", true, false});
        graph.Register(graphA);
        graph.Register(graphB);
        const auto circular = Loader::DependencyResolver(graph).Resolve("eboot.bin");
        if (circular.success || circular.circularDependencies.empty()) {
            Fail(report, "circular dependency was not reported");
        }
    }

    auto missingSymbol = process.ResolveSymbol("missing.symbol");
    if (report.failure.empty() &&
        missingSymbol.status != Loader::SymbolResolutionStatus::Missing) {
        Fail(report, "missing symbol was not reported");
    }

    Loader::SafeRelocationResolver safeResolver;
    Loader::RelocationContext safetyContext{memory, MainBase, 0x1000, nullptr};
    const auto unsupported = safeResolver.Resolve(
        {Loader::RelocationType::Unknown, 0, 0, 0, {}}, safetyContext);
    if (report.failure.empty() && unsupported.status != Loader::RelocationStatus::Unsupported) {
        Fail(report, "unsupported relocation was not rejected safely");
    }

    const auto entryPoint = process.MainModule() != nullptr
        ? process.MainModule()->EntryPoint() : InvalidGuestVirtualAddress;
    Kernel::Thread mainThread(1, process, entryPoint, 0x80000000, 0x10000);
    process.AttachThread(mainThread.Id());
    ExecutionContext context(process, mainThread);
    NativeExecutionBackend backend;
    const auto boundary = backend.Start(context);
    if (report.failure.empty() && (!context.IsValid() || !boundary.available ||
        !boundary.accepted || boundary.executed)) {
        Fail(report, "execution context boundary was not reached safely");
    }

    std::ostringstream output;
    output << "ChonkyStation4 Dynamic Module Test\n\n"
           << "Main:\neboot.bin\n\n"
           << "Dependencies:\nlibkernel.sprx\n\n"
           << "Symbols:\nExport lookup " << (symbolResult.Found() ? "PASS" : "FAIL")
           << "\n\n"
           << "Relocations:\n"
           << ((libraryRelocations.success && mainRelocations.success)
               ? "Processed safely" : "Failed") << "\n\n"
           << "Process:\nPID " << process.Id() << "\n\n"
           << "Result:\n" << (report.failure.empty() ? "SUCCESS" : "FAILURE") << "\n";
    if (!report.failure.empty()) {
        output << "\nError:\n" << report.failure << "\n";
    }
    report.log = output.str();
    report.passed = report.failure.empty();
    return report;
}

} // namespace ChonkyStation4::Core::Execution
