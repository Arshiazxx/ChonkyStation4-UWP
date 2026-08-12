#include "UpstreamExecutionSmokeTests.hpp"

#include "Core/Execution/ExecutionContext.hpp"
#include "Core/Execution/Upstream/UpstreamExecutionBackend.hpp"
#include "Core/Kernel/Process/Process.hpp"
#include "Core/Kernel/Thread/Thread.hpp"
#include "Core/Memory/GuestMemory.hpp"

#include <sstream>

namespace ChonkyStation4::Core::Execution {

UpstreamExecutionSmokeTestReport RunUpstreamExecutionSmokeTests() {
    UpstreamExecutionSmokeTestReport report;

    Memory::GuestMemory memory;
    Kernel::Process process(1, memory);
    Kernel::Thread mainThread(1, process, 0x400000, 0x600000, 0x10000);
    process.AttachThread(mainThread.Id());
    ExecutionContext context(process, mainThread);

    UpstreamExecutionBackend backend;
    const auto capabilities = backend.Platform().Probe();
    const auto boundary = backend.Start(context);

    if (!capabilities.x64Host || !capabilities.guestTls || !capabilities.hostThreads) {
        report.failure = "Xbox/UWP upstream platform seams are unavailable";
    } else if (!boundary.available || !boundary.accepted || boundary.executed) {
        report.failure = "upstream execution boundary did not stop safely before native entry";
    }

    std::ostringstream output;
    output << "ChonkyStation4 Upstream Execution Test\n\n"
           << "Architecture:\n"
           << (capabilities.x64Host ? "x86-64" : "unsupported") << "\n\n"
           << "Upstream source adapter:\n"
           << (boundary.available && boundary.accepted ? "PASS" : "FAIL") << "\n\n"
           << "Xbox/UWP platform layer:\n"
           << (capabilities.guestTls && capabilities.hostThreads ? "PASS" : "FAIL")
           << "\n\n"
           << "Native PS4 entry transfer:\n"
           << (capabilities.nativeEntryTransfer ? "AVAILABLE" : "BLOCKED") << "\n\n"
           << "Result:\n"
           << (report.failure.empty() ? "SUCCESS" : "FAILURE") << "\n";
    if (!report.failure.empty()) {
        output << "\nError:\n" << report.failure << "\n";
    }
    if (!boundary.message.empty()) {
        output << "\nBoundary:\n" << boundary.message << "\n";
    }
    report.log = output.str();
    report.passed = report.failure.empty();
    return report;
}

} // namespace ChonkyStation4::Core::Execution
