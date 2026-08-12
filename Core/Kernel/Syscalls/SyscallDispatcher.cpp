#include "SyscallDispatcher.hpp"

#include "Core/Kernel/Syscalls/Ps4SyscallStubs.hpp"
#include "Core/Kernel/Thread/Thread.hpp"

#include <iomanip>
#include <sstream>
#include <utility>

namespace ChonkyStation4::Core::Kernel {

namespace {

std::string Hex(std::uint64_t value) {
    std::ostringstream text;
    text << "0x" << std::uppercase << std::setfill('0') << std::setw(2)
         << std::hex << value;
    return text.str();
}

} // namespace

const char* SyscallName(Syscall syscall) noexcept {
    switch (syscall) {
    case Syscall::ProcessExit:
        return "sceKernelExitProcess";
    case Syscall::ProcessGetInfo:
        return "sceKernelGetProcessInfo";
    case Syscall::ThreadCreate:
        return "sceKernelCreateThread";
    case Syscall::ThreadStart:
        return "sceKernelStartThread";
    case Syscall::ThreadExit:
        return "sceKernelExitThread";
    case Syscall::MemoryAllocate:
        return "sceKernelAllocateMemory";
    case Syscall::MemoryRelease:
        return "sceKernelReleaseMemory";
    }
    return "Unknown";
}

const char* SyscallStatusName(SyscallStatus status) noexcept {
    switch (status) {
    case SyscallStatus::Implemented:
        return "Implemented";
    case SyscallStatus::Stub:
        return "Stub";
    }
    return "Unknown";
}

SyscallDispatcher::SyscallDispatcher() {
    Register({
        static_cast<SyscallNumber>(Syscall::ProcessExit),
        SyscallName(Syscall::ProcessExit),
        "Process",
        SyscallStatus::Implemented,
        1,
    }, &Ps4SyscallStubs::ProcessExit);
    Register({
        static_cast<SyscallNumber>(Syscall::ProcessGetInfo),
        SyscallName(Syscall::ProcessGetInfo),
        "Process",
        SyscallStatus::Stub,
        0,
    }, &Ps4SyscallStubs::GetProcessInfo);
    Register({
        static_cast<SyscallNumber>(Syscall::ThreadCreate),
        SyscallName(Syscall::ThreadCreate),
        "Thread",
        SyscallStatus::Stub,
        3,
    }, &Ps4SyscallStubs::CreateThread);
    Register({
        static_cast<SyscallNumber>(Syscall::ThreadStart),
        SyscallName(Syscall::ThreadStart),
        "Thread",
        SyscallStatus::Stub,
        1,
    }, &Ps4SyscallStubs::StartThread);
    Register({
        static_cast<SyscallNumber>(Syscall::ThreadExit),
        SyscallName(Syscall::ThreadExit),
        "Thread",
        SyscallStatus::Stub,
        1,
    }, &Ps4SyscallStubs::ExitThread);
    Register({
        static_cast<SyscallNumber>(Syscall::MemoryAllocate),
        SyscallName(Syscall::MemoryAllocate),
        "Memory",
        SyscallStatus::Stub,
        1,
    }, &Ps4SyscallStubs::AllocateMemory);
    Register({
        static_cast<SyscallNumber>(Syscall::MemoryRelease),
        SyscallName(Syscall::MemoryRelease),
        "Memory",
        SyscallStatus::Stub,
        1,
    }, &Ps4SyscallStubs::ReleaseMemory);
}

bool SyscallDispatcher::Register(
    SyscallNumber number,
    const std::string& handlerName,
    SyscallHandler handler,
    std::string* error) {
    return Register({number, handlerName, "Custom", SyscallStatus::Implemented, 0},
        std::move(handler), error);
}

bool SyscallDispatcher::Register(
    const SyscallMetadata& metadata,
    SyscallHandler handler,
    std::string* error) {
    return registry_.Register(metadata, std::move(handler), error);
}

SyscallResult SyscallDispatcher::Dispatch(
    SyscallNumber number,
    SyscallContext& context) const {
    SyscallResult result;
    const auto* found = registry_.Find(number);
    if (found == nullptr) {
        result.error = "no handler registered for syscall " + Hex(number);
        result.handlerName = "Unknown";
    } else {
        result.handlerName = found->metadata.name;
        result = found->handler(context);
        if (result.handlerName.empty()) {
            result.handlerName = found->metadata.name;
        }
    }

    std::ostringstream log;
    log << "ChonkyStation4 Syscall\n\n"
        << "Number:\n" << Hex(number) << "\n\n"
        << "Handler:\n" << result.handlerName << "\n\n"
        << "Status:\n";
    if (found != nullptr) {
        log << SyscallStatusName(found->metadata.status);
    } else {
        log << "Unknown";
    }
    log << "\n\n"
        << "Result:\n" << (result.success ? "Success" : "Failure") << "\n";
    if (!result.error.empty()) {
        log << "\nError:\n" << result.error << "\n";
    }
    result.log = log.str();
    return result;
}

const SyscallMetadata* SyscallDispatcher::Metadata(
    SyscallNumber number) const noexcept {
    return registry_.Metadata(number);
}

const SyscallRegistry& SyscallDispatcher::Registry() const noexcept {
    return registry_;
}

SyscallResult SyscallDispatcher::Dispatch(
    Syscall syscall,
    SyscallContext& context) const {
    return Dispatch(static_cast<SyscallNumber>(syscall), context);
}

SyscallResult SyscallDispatcher::ProcessExit(SyscallContext& context) {
    return Ps4SyscallStubs::ProcessExit(context);
}

} // namespace ChonkyStation4::Core::Kernel
