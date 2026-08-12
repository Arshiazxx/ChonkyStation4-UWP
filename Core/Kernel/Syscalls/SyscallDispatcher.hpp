#pragma once

#include "Core/Kernel/Syscalls/SyscallRegistry.hpp"

#include <string>

namespace ChonkyStation4::Core::Kernel {

class Thread;

class SyscallDispatcher final {
public:
    SyscallDispatcher();

    bool Register(
        SyscallNumber number,
        const std::string& handlerName,
        SyscallHandler handler,
        std::string* error = nullptr);

    bool Register(
        const SyscallMetadata& metadata,
        SyscallHandler handler,
        std::string* error = nullptr);

    SyscallResult Dispatch(SyscallNumber number, SyscallContext& context) const;
    SyscallResult Dispatch(Syscall syscall, SyscallContext& context) const;

    const SyscallMetadata* Metadata(SyscallNumber number) const noexcept;
    const SyscallRegistry& Registry() const noexcept;

    static SyscallResult ProcessExit(SyscallContext& context);

private:
    SyscallRegistry registry_;
};

const char* SyscallName(Syscall syscall) noexcept;

} // namespace ChonkyStation4::Core::Kernel
