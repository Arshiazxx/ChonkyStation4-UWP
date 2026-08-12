#pragma once

#include "Core/Loader/Elf64Loader.hpp"
#include "Core/Memory/GuestMemory.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace ChonkyStation4::Core::Kernel {

using ProcessId = std::uint32_t;
using ThreadId = std::uint32_t;

enum class ProcessState {
    Created,
    Ready,
    Running,
    Terminated,
    Faulted,
};

struct LoadedExecutable {
    bool loaded = false;
    std::string path;
    Loader::ElfArchitecture architecture = Loader::ElfArchitecture::Unknown;
    std::uint64_t entryPoint = 0;
    std::vector<Loader::ElfLoadSegment> loadableSegments;
};

class Process final {
public:
    Process(ProcessId id, Memory::GuestMemory& addressSpace) noexcept;

    ProcessId Id() const noexcept;
    Memory::GuestMemory& AddressSpace() noexcept;
    const Memory::GuestMemory& AddressSpace() const noexcept;

    ProcessState State() const noexcept;
    void SetState(ProcessState state) noexcept;

    bool LoadExecutable(const std::string& path, std::string* error = nullptr);
    const LoadedExecutable& Executable() const noexcept;

    void AttachThread(ThreadId id);
    const std::vector<ThreadId>& ThreadIds() const noexcept;

    void Terminate(std::int64_t exitCode = 0) noexcept;
    void Fault(const std::string& message);
    std::int64_t ExitCode() const noexcept;
    const std::string& ExceptionMessage() const noexcept;

private:
    ProcessId id_ = 0;
    Memory::GuestMemory* addressSpace_ = nullptr;
    LoadedExecutable executable_{};
    ProcessState state_ = ProcessState::Created;
    std::vector<ThreadId> threadIds_;
    std::int64_t exitCode_ = 0;
    std::string exceptionMessage_;
};

} // namespace ChonkyStation4::Core::Kernel
