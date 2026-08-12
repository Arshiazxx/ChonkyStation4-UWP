#pragma once

#include "Core/CPU/CpuExecutor.hpp"

#include <cstdint>
#include <string>

namespace ChonkyStation4::Core::Kernel {

class Process;
class Thread;

enum class ExceptionKind {
    CpuFault,
    MemoryFault,
    InvalidInstruction,
    SyscallTransition,
};

struct RuntimeException {
    ExceptionKind kind = ExceptionKind::CpuFault;
    std::uint64_t instructionPointer = 0;
    std::uint64_t syscallNumber = 0;
    std::string message;
};

enum class ExceptionDisposition {
    Continue,
    TerminateThread,
    TerminateProcess,
};

class IExceptionHandler {
public:
    virtual ~IExceptionHandler() = default;
    virtual ExceptionDisposition Handle(
        const RuntimeException& exception,
        Process& process,
        Thread& thread,
        std::string& log) = 0;
};

class DefaultExceptionHandler final : public IExceptionHandler {
public:
    ExceptionDisposition Handle(
        const RuntimeException& exception,
        Process& process,
        Thread& thread,
        std::string& log) override;
};

class ExceptionBoundary final {
public:
    explicit ExceptionBoundary(IExceptionHandler& handler) noexcept;

    ExceptionDisposition HandleCpuFailure(
        Process& process,
        Thread& thread,
        const CPU::CpuExecutionReport& report,
        std::string* log = nullptr) const;

    ExceptionDisposition HandleMemoryFault(
        Process& process,
        Thread& thread,
        std::uint64_t instructionPointer,
        const std::string& message,
        std::string* log = nullptr) const;

    ExceptionDisposition HandleInvalidInstruction(
        Process& process,
        Thread& thread,
        std::uint64_t instructionPointer,
        const std::string& message,
        std::string* log = nullptr) const;

    ExceptionDisposition HandleSyscallTransition(
        Process& process,
        Thread& thread,
        std::uint64_t syscallNumber,
        std::string* log = nullptr) const;

private:
    ExceptionDisposition Dispatch(
        const RuntimeException& exception,
        Process& process,
        Thread& thread,
        std::string* log) const;

    IExceptionHandler& handler_;
};

const char* ExceptionKindName(ExceptionKind kind) noexcept;

} // namespace ChonkyStation4::Core::Kernel
