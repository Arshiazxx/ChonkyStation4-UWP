#pragma once

#include "Core/Execution/Upstream/UpstreamExecutionTypes.hpp"

#include <functional>
#include <string>

namespace ChonkyStation4::Core::Execution::Upstream {

// Xbox/UWP adaptation of the host facilities used by upstream's loader,
// linker, App, and OS::Thread layers. It owns no PS4 policy and does not
// pretend that a guest address is a callable host pointer.
class XboxUwpExecutionPlatform final {
public:
    const char* Name() const noexcept;

    PlatformCapabilities Probe() const;

    ExecutableMemoryBlock AllocateExecutableMemory(
        std::size_t size,
        std::string* error = nullptr) const;
    bool ProtectMemory(
        void* address,
        std::size_t size,
        MemoryProtection protection,
        std::string* error = nullptr) const;
    bool ReleaseExecutableMemory(
        ExecutableMemoryBlock block,
        std::string* error = nullptr) const;

    void SetGuestTlsPointer(void* pointer) const noexcept;
    void* GuestTlsPointer() const noexcept;

    bool RunHostThread(
        const std::function<void()>& entry,
        std::string* error = nullptr) const;

    void SetApplicationDataRoot(std::string path) const;
    std::string ApplicationDataRoot() const;

    // The upstream bridge needs PS4 SysV register/stack setup followed by a
    // native jump. MSVC x64 has no inline assembly, and Xbox/UWP executable
    // memory policy has not yet been validated for this bridge. Keep this
    // failure explicit so a guest address can never be called accidentally.
    EntryTransferResult TransferToEntry(const EntryState& state) const;
};

} // namespace ChonkyStation4::Core::Execution::Upstream
