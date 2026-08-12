#include "SyscallTransition.hpp"

#include "Core/ABI/Arguments/AbiContext.hpp"
#include "Core/ABI/CallingConvention/X86_64CallingConvention.hpp"
#include "Core/ABI/ReturnValues/ReturnValue.hpp"
#include "Core/Kernel/Process/Process.hpp"
#include "Core/Kernel/Scheduler/Scheduler.hpp"
#include "Core/Kernel/Thread/Thread.hpp"

#include <limits>
#include <iomanip>
#include <sstream>

namespace ChonkyStation4::Core::ABI {

namespace {

constexpr std::uint64_t AbiErrorValue = (std::numeric_limits<std::uint64_t>::max)();

std::string Hex(std::uint64_t value) {
    std::ostringstream text;
    text << "0x" << std::uppercase << std::setfill('0') << std::setw(3)
         << std::hex << value;
    return text.str();
}

void AppendArguments(std::ostringstream& log, const AbiContext& abi) {
    log << "Arguments:\n";
    for (std::size_t index = 0;
         index < X86_64CallingConvention::RegisterArgumentCount;
         ++index) {
        std::uint64_t value = 0;
        if (abi.TryGetArgument(index, value)) {
            log << "arg" << index << " = " << Hex(value) << "\n";
        }
    }
}

} // namespace

SyscallTransitionReport SyscallTransition::Invoke(
    Kernel::Process& process,
    Kernel::Thread& thread,
    Kernel::SyscallDispatcher& dispatcher,
    Kernel::Scheduler* scheduler) const {
    SyscallTransitionReport report;
    AbiContext abi(thread.Cpu());
    report.number = abi.SyscallNumber();
    const auto* metadata = dispatcher.Metadata(report.number);

    Kernel::SyscallContext context{
        process,
        thread,
        thread.Cpu(),
        &abi,
        scheduler,
    };
    report.result = dispatcher.Dispatch(report.number, context);

    const ReturnValue returnValue{
        report.result.success ? report.result.value : AbiErrorValue};
    WriteReturnValue(thread.Cpu(), returnValue);

    std::ostringstream log;
    log << "ChonkyStation4 ABI\n\n"
        << "Syscall:\n"
        << (metadata == nullptr ? "Unknown" : metadata->name) << "\n\n";
    AppendArguments(log, abi);
    log << "\nResult:\n"
        << (report.result.success ? "SUCCESS" : "FAILURE") << "\n\n"
        << "Return:\n" << Hex(returnValue.raw) << "\n";
    if (!report.result.error.empty()) {
        log << "\nError:\n" << report.result.error << "\n";
        report.error = report.result.error;
    }
    log << "\nDispatcher:\n" << report.result.log;
    report.log = log.str();
    report.success = report.result.success;
    return report;
}

} // namespace ChonkyStation4::Core::ABI
