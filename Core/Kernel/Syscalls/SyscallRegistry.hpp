#pragma once

#include "Core/Kernel/Syscalls/SyscallTypes.hpp"

#include <cstddef>
#include <string>
#include <unordered_map>

namespace ChonkyStation4::Core::Kernel {

struct SyscallEntry {
    SyscallMetadata metadata;
    SyscallHandler handler;
};

class SyscallRegistry final {
public:
    bool Register(
        const SyscallMetadata& metadata,
        SyscallHandler handler,
        std::string* error = nullptr);

    const SyscallEntry* Find(SyscallNumber number) const noexcept;
    const SyscallMetadata* Metadata(SyscallNumber number) const noexcept;
    std::size_t Size() const noexcept;

private:
    std::unordered_map<SyscallNumber, SyscallEntry> entries_;
};

} // namespace ChonkyStation4::Core::Kernel
