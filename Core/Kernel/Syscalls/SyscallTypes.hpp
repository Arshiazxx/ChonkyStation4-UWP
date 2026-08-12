#pragma once

#include "Core/CPU/CpuState.hpp"
#include "Core/Kernel/Process/Process.hpp"

#include <cstdint>
#include <functional>
#include <string>

namespace ChonkyStation4::Core::ABI {
class AbiContext;
}

namespace ChonkyStation4::Core::Kernel {

class Scheduler;
class Thread;

using SyscallNumber = std::uint64_t;

enum class Syscall : SyscallNumber {
    ProcessExit = 0x01,
    ProcessGetInfo = 0x02,
    ThreadCreate = 0x10,
    ThreadStart = 0x11,
    ThreadExit = 0x12,
    MemoryAllocate = 0x20,
    MemoryRelease = 0x21,
};

enum class SyscallStatus {
    Implemented,
    Stub,
};

struct SyscallMetadata {
    SyscallNumber number = 0;
    std::string name;
    std::string category;
    SyscallStatus status = SyscallStatus::Stub;
    std::uint32_t argumentCount = 0;
};

struct SyscallContext {
    Process& process;
    Thread& thread;
    CPU::CpuState& cpu;
    ABI::AbiContext* abi = nullptr;
    Scheduler* scheduler = nullptr;
};

struct SyscallResult {
    bool success = false;
    std::uint64_t value = 0;
    std::string handlerName;
    std::string error;
    std::string log;
};

using SyscallHandler = std::function<SyscallResult(SyscallContext&)>;

const char* SyscallStatusName(SyscallStatus status) noexcept;

} // namespace ChonkyStation4::Core::Kernel
