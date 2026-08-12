#pragma once

#include "Core/Loader/Elf64Loader.hpp"
#include "Core/Loader/Module/ModuleManager.hpp"
#include "Core/Memory/GuestMemory.hpp"

#include <cstdint>
#include <limits>
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

    bool RegisterModule(
        const Loader::LoadedModule& module,
        std::string* error = nullptr);
    bool RegisterMainModule(
        const Loader::LoadedModule& module,
        std::string* error = nullptr);
    const std::vector<Loader::LoadedModule>& Modules() const noexcept;
    const Loader::LoadedModule* MainModule() const noexcept;
    Loader::ModuleManager& ModuleManager() noexcept;
    const Loader::ModuleManager& ModuleManager() const noexcept;

    bool LoadModule(
        const std::string& name,
        const std::string& path,
        Loader::ModuleKind kind = Loader::ModuleKind::SharedObject,
        std::string* error = nullptr);
    bool LoadModuleBytes(
        const std::string& name,
        const std::string& sourcePath,
        const std::vector<std::uint8_t>& bytes,
        Loader::ModuleKind kind = Loader::ModuleKind::SharedObject,
        std::string* error = nullptr);
    bool UnloadModule(const std::string& name, std::string* error = nullptr);
    const Loader::LoadedModule* FindModule(const std::string& name) const noexcept;
    Loader::SymbolResolutionResult ResolveSymbol(const std::string& name) const;

    void AttachThread(ThreadId id);
    const std::vector<ThreadId>& ThreadIds() const noexcept;

    void Terminate(std::int64_t exitCode = 0) noexcept;
    void Fault(const std::string& message);
    std::int64_t ExitCode() const noexcept;
    const std::string& ExceptionMessage() const noexcept;

private:
    ProcessId id_ = 0;
    Memory::GuestMemory* addressSpace_ = nullptr;
    Loader::ModuleManager moduleManager_;
    LoadedExecutable executable_{};
    ProcessState state_ = ProcessState::Created;
    std::vector<ThreadId> threadIds_;
    std::int64_t exitCode_ = 0;
    std::string exceptionMessage_;
};

} // namespace ChonkyStation4::Core::Kernel
