#include "ExceptionBoundary.hpp"

#include "Core/Kernel/Process/Process.hpp"
#include "Core/Kernel/Thread/Thread.hpp"

#include <iomanip>
#include <sstream>

namespace ChonkyStation4::Core::Kernel {

namespace {

std::string Hex(std::uint64_t value) {
    std::ostringstream text;
    text << "0x" << std::uppercase << std::hex << value;
    return text.str();
}

void AppendLine(std::string& output, const std::string& line) {
    output += line;
    output.push_back('\n');
}

ExceptionKind MapCpuException(CPU::CpuExceptionKind kind) {
    switch (kind) {
    case CPU::CpuExceptionKind::MemoryFault:
        return ExceptionKind::MemoryFault;
    case CPU::CpuExceptionKind::InvalidInstruction:
        return ExceptionKind::InvalidInstruction;
    case CPU::CpuExceptionKind::AddressFault:
        return ExceptionKind::MemoryFault;
    case CPU::CpuExceptionKind::None:
    case CPU::CpuExceptionKind::ExecutionFault:
    case CPU::CpuExceptionKind::StepLimit:
        return ExceptionKind::CpuFault;
    }
    return ExceptionKind::CpuFault;
}

} // namespace

const char* ExceptionKindName(ExceptionKind kind) noexcept {
    switch (kind) {
    case ExceptionKind::CpuFault:
        return "CPU fault";
    case ExceptionKind::MemoryFault:
        return "Memory fault";
    case ExceptionKind::InvalidInstruction:
        return "Invalid instruction";
    case ExceptionKind::SyscallTransition:
        return "Syscall transition";
    }
    return "Unknown exception";
}

ExceptionDisposition DefaultExceptionHandler::Handle(
    const RuntimeException& exception,
    Process& process,
    Thread& thread,
    std::string& log) {
    AppendLine(log, "ChonkyStation4 Exception");
    AppendLine(log, "");
    AppendLine(log, "Type:");
    AppendLine(log, ExceptionKindName(exception.kind));
    AppendLine(log, "RIP: " + Hex(exception.instructionPointer));
    AppendLine(log, "Message:");
    AppendLine(log, exception.message);

    if (exception.kind == ExceptionKind::SyscallTransition) {
        AppendLine(log, "Disposition: Continue");
        return ExceptionDisposition::Continue;
    }

    thread.MarkFaulted(exception.message);
    process.Fault(exception.message);
    AppendLine(log, "Disposition: Process faulted");
    return ExceptionDisposition::TerminateProcess;
}

ExceptionBoundary::ExceptionBoundary(IExceptionHandler& handler) noexcept
    : handler_(handler) {}

ExceptionDisposition ExceptionBoundary::HandleCpuFailure(
    Process& process,
    Thread& thread,
    const CPU::CpuExecutionReport& report,
    std::string* log) const {
    RuntimeException exception;
    exception.kind = MapCpuException(report.exceptionKind);
    exception.instructionPointer = thread.Cpu().instructionPointer;
    exception.message = report.error.empty()
        ? thread.Cpu().exceptionMessage
        : report.error;
    return Dispatch(exception, process, thread, log);
}

ExceptionDisposition ExceptionBoundary::HandleMemoryFault(
    Process& process,
    Thread& thread,
    std::uint64_t instructionPointer,
    const std::string& message,
    std::string* log) const {
    RuntimeException exception{
        ExceptionKind::MemoryFault,
        instructionPointer,
        0,
        message,
    };
    return Dispatch(exception, process, thread, log);
}

ExceptionDisposition ExceptionBoundary::HandleInvalidInstruction(
    Process& process,
    Thread& thread,
    std::uint64_t instructionPointer,
    const std::string& message,
    std::string* log) const {
    RuntimeException exception{
        ExceptionKind::InvalidInstruction,
        instructionPointer,
        0,
        message,
    };
    return Dispatch(exception, process, thread, log);
}

ExceptionDisposition ExceptionBoundary::HandleSyscallTransition(
    Process& process,
    Thread& thread,
    std::uint64_t syscallNumber,
    std::string* log) const {
    RuntimeException exception{
        ExceptionKind::SyscallTransition,
        thread.Cpu().instructionPointer,
        syscallNumber,
        "transition to syscall " + Hex(syscallNumber),
    };
    return Dispatch(exception, process, thread, log);
}

ExceptionDisposition ExceptionBoundary::Dispatch(
    const RuntimeException& exception,
    Process& process,
    Thread& thread,
    std::string* log) const {
    std::string localLog;
    const auto disposition = handler_.Handle(exception, process, thread, localLog);
    if (log != nullptr) {
        *log += localLog;
    }
    return disposition;
}

} // namespace ChonkyStation4::Core::Kernel
