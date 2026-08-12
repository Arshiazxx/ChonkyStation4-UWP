#pragma once

#include "Core/Kernel/Syscalls/SyscallDispatcher.hpp"

#include <cstdint>
#include <string>

namespace ChonkyStation4::Core::Kernel {
class Scheduler;
class Process;
class Thread;
}

namespace ChonkyStation4::Core::ABI {

struct SyscallTransitionReport {
    bool success = false;
    Kernel::SyscallNumber number = 0;
    Kernel::SyscallResult result;
    std::string log;
    std::string error;
};

class SyscallTransition final {
public:
    SyscallTransitionReport Invoke(
        Kernel::Process& process,
        Kernel::Thread& thread,
        Kernel::SyscallDispatcher& dispatcher,
        Kernel::Scheduler* scheduler = nullptr) const;
};

} // namespace ChonkyStation4::Core::ABI
