#include "XboxUwpExecutionPlatform.hpp"

#if defined(_WIN32)
#define NOMINMAX
#include <windows.h>
#endif

#include <mutex>
#include <thread>
#include <utility>

namespace ChonkyStation4::Core::Execution::Upstream {

namespace {

thread_local void* guestTlsPointer = nullptr;
std::mutex applicationDataMutex;
std::string applicationDataRoot;

void SetError(std::string* error, std::string message) {
    if (error != nullptr) {
        *error = std::move(message);
    }
}

#if defined(_WIN32)
DWORD ToNativeProtection(MemoryProtection protection) {
    switch (protection) {
    case MemoryProtection::ReadWrite:
        return PAGE_READWRITE;
    case MemoryProtection::ReadExecute:
        return PAGE_EXECUTE_READ;
    case MemoryProtection::ReadWriteExecute:
        return PAGE_EXECUTE_READWRITE;
    }
    return PAGE_NOACCESS;
}
#endif

} // namespace

const char* XboxUwpExecutionPlatform::Name() const noexcept {
    return "Xbox/UWP upstream execution platform adapter";
}

PlatformCapabilities XboxUwpExecutionPlatform::Probe() const {
    PlatformCapabilities capabilities;
#if defined(_M_X64) || defined(__x86_64__) || defined(__amd64__)
    capabilities.x64Host = true;
#endif

    // These facilities are available to the adapter, but the guest entry
    // bridge remains deliberately disabled until its ABI and policy contracts
    // are independently validated on Xbox/UWP.
    capabilities.guestTls = true;
    capabilities.hostThreads = true;
    capabilities.filesystemPaths = !ApplicationDataRoot().empty();
    capabilities.platformServices = false;
    capabilities.executableMemory = false;
    capabilities.memoryProtection = false;
    capabilities.nativeEntryTransfer = false;
    capabilities.detail =
        "x86-64 host/thread/TLS seams are available; native guest entry transfer "
        "is blocked pending the Xbox SysV bridge and executable-code capability gate";
    return capabilities;
}

ExecutableMemoryBlock XboxUwpExecutionPlatform::AllocateExecutableMemory(
    std::size_t size,
    std::string* error) const {
    if (size == 0) {
        SetError(error, "executable memory size must be non-zero");
        return {};
    }

#if defined(_WIN32)
#if defined(CHONKYSTATION4_XBOX_UWP)
    // VirtualAllocFromApp is the UWP-visible allocation surface. This method
    // is intentionally opt-in and is not called by Probe or the backend.
    void* address = VirtualAllocFromApp(
        nullptr,
        static_cast<SIZE_T>(size),
        MEM_RESERVE | MEM_COMMIT,
        PAGE_READWRITE);
#else
    // The Core smoke runner is a desktop validation process. Keep its link
    // surface portable while the Xbox project selects the FromApp API above.
    void* address = VirtualAlloc(
        nullptr,
        static_cast<SIZE_T>(size),
        MEM_RESERVE | MEM_COMMIT,
        PAGE_READWRITE);
#endif
    if (address == nullptr) {
        SetError(error, "host execution buffer allocation failed");
        return {};
    }
    return {address, size};
#else
    SetError(error, "Xbox/UWP executable memory is unavailable on this platform");
    return {};
#endif
}

bool XboxUwpExecutionPlatform::ProtectMemory(
    void* address,
    std::size_t size,
    MemoryProtection protection,
    std::string* error) const {
    if (address == nullptr || size == 0) {
        SetError(error, "memory protection requires a valid range");
        return false;
    }

#if defined(_WIN32)
    DWORD oldProtection = 0;
#if defined(CHONKYSTATION4_XBOX_UWP)
    const BOOL protectedMemory = VirtualProtectFromApp(
        address,
        static_cast<SIZE_T>(size),
        ToNativeProtection(protection),
        &oldProtection);
#else
    const BOOL protectedMemory = VirtualProtect(
        address,
        static_cast<SIZE_T>(size),
        ToNativeProtection(protection),
        &oldProtection);
#endif
    if (!protectedMemory) {
        SetError(error, "host memory protection change failed for execution buffer");
        return false;
    }
    return true;
#else
    SetError(error, "Xbox/UWP memory protection is unavailable on this platform");
    return false;
#endif
}

bool XboxUwpExecutionPlatform::ReleaseExecutableMemory(
    ExecutableMemoryBlock block,
    std::string* error) const {
    if (!block.Valid()) {
        SetError(error, "cannot release an invalid execution buffer");
        return false;
    }

#if defined(_WIN32)
    if (!VirtualFree(block.address, 0, MEM_RELEASE)) {
        SetError(error, "VirtualFree failed for execution buffer");
        return false;
    }
    return true;
#else
    SetError(error, "Xbox/UWP executable memory is unavailable on this platform");
    return false;
#endif
}

void XboxUwpExecutionPlatform::SetGuestTlsPointer(void* pointer) const noexcept {
    guestTlsPointer = pointer;
}

void* XboxUwpExecutionPlatform::GuestTlsPointer() const noexcept {
    return guestTlsPointer;
}

bool XboxUwpExecutionPlatform::RunHostThread(
    const std::function<void()>& entry,
    std::string* error) const {
    if (!entry) {
        SetError(error, "host thread entry is empty");
        return false;
    }

    try {
        std::thread hostThread(entry);
        hostThread.join();
        return true;
    } catch (const std::system_error& exception) {
        SetError(error, std::string("host thread creation failed: ") + exception.what());
        return false;
    }
}

void XboxUwpExecutionPlatform::SetApplicationDataRoot(std::string path) const {
    std::lock_guard<std::mutex> lock(applicationDataMutex);
    applicationDataRoot = std::move(path);
}

std::string XboxUwpExecutionPlatform::ApplicationDataRoot() const {
    std::lock_guard<std::mutex> lock(applicationDataMutex);
    return applicationDataRoot;
}

EntryTransferResult XboxUwpExecutionPlatform::TransferToEntry(const EntryState& state) const {
    EntryTransferResult result;
    if (state.nativeEntryPoint == 0) {
        result.error = "module has no mapped native entry point";
        result.message = result.error;
        return result;
    }

    result.error =
        "native entry transfer is blocked: the Xbox/UWP PS4 SysV bridge, stack setup, "
        "and executable-code policy are not enabled";
    result.message = result.error;
    return result;
}

} // namespace ChonkyStation4::Core::Execution::Upstream
