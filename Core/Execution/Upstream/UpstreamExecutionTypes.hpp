#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

namespace ChonkyStation4::Core::Execution::Upstream {

// This is the state that upstream App::initAndJumpToEntry prepares before
// transferring control to Module::entry. It is deliberately kept separate
// from the synthetic CpuState and from a guest virtual address.
struct EntryState final {
    std::uintptr_t nativeEntryPoint = 0;
    std::uintptr_t stackPointer = 0;
    std::uintptr_t parameterBlock = 0;
    std::uintptr_t exitHandler = 0;
    std::string moduleName;
};

struct PlatformCapabilities final {
    bool x64Host = false;
    bool executableMemory = false;
    bool memoryProtection = false;
    bool guestTls = false;
    bool hostThreads = false;
    bool filesystemPaths = false;
    bool platformServices = false;
    bool nativeEntryTransfer = false;
    std::string detail;
};

struct EntryTransferResult final {
    bool supported = false;
    bool started = false;
    bool completed = false;
    std::string message;
    std::string error;
};

struct ExecutableMemoryBlock final {
    void* address = nullptr;
    std::size_t size = 0;

    bool Valid() const noexcept {
        return address != nullptr && size != 0;
    }
};

enum class MemoryProtection {
    ReadWrite,
    ReadExecute,
    ReadWriteExecute,
};

} // namespace ChonkyStation4::Core::Execution::Upstream
