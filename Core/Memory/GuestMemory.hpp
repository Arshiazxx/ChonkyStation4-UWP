#pragma once

#include "Core/Common/VirtualAddress.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace ChonkyStation4::Core::Memory {

enum class MemoryPermission : std::uint32_t {
    None = 0,
    Read = 1u << 0,
    Write = 1u << 1,
    Execute = 1u << 2,
};

using MemoryPermissions = std::uint32_t;

constexpr MemoryPermissions ToPermissions(MemoryPermission permission) {
    return static_cast<MemoryPermissions>(permission);
}

struct MemoryRegion {
    GuestVirtualAddress baseAddress = InvalidGuestVirtualAddress;
    std::uint64_t size = 0;
    MemoryPermissions permissions = ToPermissions(MemoryPermission::None);
    std::string name;
};

class GuestMemory final {
public:
    GuestMemory() = default;
    GuestMemory(const GuestMemory&) = delete;
    GuestMemory& operator=(const GuestMemory&) = delete;

    bool Map(
        const MemoryRegion& region,
        const void* initialData = nullptr,
        std::size_t initialDataSize = 0,
        std::string* error = nullptr);
    bool Unmap(GuestVirtualAddress baseAddress, std::string* error = nullptr);

    bool Read(
        GuestVirtualAddress address,
        void* destination,
        std::size_t size,
        std::string* error = nullptr) const;
    bool Write(
        GuestVirtualAddress address,
        const void* source,
        std::size_t size,
        std::string* error = nullptr);

    const std::vector<MemoryRegion>& Regions() const noexcept;
    void Clear() noexcept;

private:
    std::size_t FindRegion(GuestVirtualAddress address) const noexcept;
    bool Access(
        GuestVirtualAddress address,
        void* buffer,
        std::size_t size,
        bool write,
        std::string* error);

    std::vector<MemoryRegion> regions_;
    std::vector<std::vector<std::uint8_t>> storage_;
};

} // namespace ChonkyStation4::Core::Memory
