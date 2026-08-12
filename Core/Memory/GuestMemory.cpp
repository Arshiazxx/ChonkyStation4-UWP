#include "GuestMemory.hpp"

#include <algorithm>
#include <cstring>
#include <limits>
#include <new>
#include <utility>

namespace ChonkyStation4::Core::Memory {

namespace {

constexpr std::size_t NoRegion = (std::numeric_limits<std::size_t>::max)();

bool AddOverflows(GuestVirtualAddress base, std::uint64_t size) {
    return size > (std::numeric_limits<GuestVirtualAddress>::max)() - base;
}

void SetError(std::string* error, const char* message) {
    if (error != nullptr) {
        *error = message;
    }
}

bool HasPermission(MemoryPermissions permissions, MemoryPermission permission) {
    return (permissions & ToPermissions(permission)) != 0;
}

} // namespace

bool GuestMemory::Map(
    const MemoryRegion& region,
    const void* initialData,
    std::size_t initialDataSize,
    std::string* error) {
    if (region.baseAddress == InvalidGuestVirtualAddress) {
        SetError(error, "guest memory regions cannot start at address zero");
        return false;
    }
    if (region.size == 0) {
        SetError(error, "guest memory regions must have a non-zero size");
        return false;
    }
    if (AddOverflows(region.baseAddress, region.size)) {
        SetError(error, "guest memory region address range overflows");
        return false;
    }
    if (region.size > (std::numeric_limits<std::size_t>::max)()) {
        SetError(error, "guest memory region is too large for this host");
        return false;
    }
    if (initialDataSize > region.size) {
        SetError(error, "initial segment data is larger than the mapped region");
        return false;
    }
    if (initialDataSize != 0 && initialData == nullptr) {
        SetError(error, "initial segment data is null");
        return false;
    }

    const auto regionEnd = region.baseAddress + region.size;
    for (const auto& existing : regions_) {
        const auto existingEnd = existing.baseAddress + existing.size;
        if (region.baseAddress < existingEnd && existing.baseAddress < regionEnd) {
            SetError(error, "guest memory region overlaps an existing mapping");
            return false;
        }
    }

    try {
        std::vector<std::uint8_t> bytes(static_cast<std::size_t>(region.size), 0);
        if (initialDataSize != 0) {
            std::memcpy(bytes.data(), initialData, initialDataSize);
        }
        regions_.push_back(region);
        try {
            storage_.push_back(std::move(bytes));
        } catch (...) {
            regions_.pop_back();
            throw;
        }
    } catch (const std::bad_alloc&) {
        SetError(error, "unable to allocate guest memory backing storage");
        return false;
    }

    return true;
}

bool GuestMemory::Unmap(GuestVirtualAddress baseAddress, std::string* error) {
    for (std::size_t index = 0; index < regions_.size(); ++index) {
        if (regions_[index].baseAddress == baseAddress) {
            regions_.erase(regions_.begin() + static_cast<std::ptrdiff_t>(index));
            storage_.erase(storage_.begin() + static_cast<std::ptrdiff_t>(index));
            return true;
        }
    }

    SetError(error, "guest memory region was not found");
    return false;
}

bool GuestMemory::Read(
    GuestVirtualAddress address,
    void* destination,
    std::size_t size,
    std::string* error) const {
    return const_cast<GuestMemory*>(this)->Access(address, destination, size, false, error);
}

bool GuestMemory::Write(
    GuestVirtualAddress address,
    const void* source,
    std::size_t size,
    std::string* error) {
    return Access(address, const_cast<void*>(source), size, true, error);
}

const std::vector<MemoryRegion>& GuestMemory::Regions() const noexcept {
    return regions_;
}

void GuestMemory::Clear() noexcept {
    regions_.clear();
    storage_.clear();
}

std::size_t GuestMemory::FindRegion(GuestVirtualAddress address) const noexcept {
    for (std::size_t index = 0; index < regions_.size(); ++index) {
        const auto& region = regions_[index];
        const auto regionEnd = region.baseAddress + region.size;
        if (address >= region.baseAddress && address < regionEnd) {
            return index;
        }
    }
    return NoRegion;
}

bool GuestMemory::Access(
    GuestVirtualAddress address,
    void* buffer,
    std::size_t size,
    bool write,
    std::string* error) {
    if (size == 0) {
        return true;
    }
    if (buffer == nullptr) {
        SetError(error, "guest memory access buffer is null");
        return false;
    }
    if (AddOverflows(address, static_cast<std::uint64_t>(size))) {
        SetError(error, "guest memory access range overflows");
        return false;
    }

    auto* bytes = static_cast<std::uint8_t*>(buffer);
    std::size_t remaining = size;
    auto currentAddress = address;
    while (remaining != 0) {
        const auto index = FindRegion(currentAddress);
        if (index == NoRegion) {
            SetError(error, "guest memory access reaches an unmapped address");
            return false;
        }

        const auto& region = regions_[index];
        const auto required = write ? MemoryPermission::Write : MemoryPermission::Read;
        if (!HasPermission(region.permissions, required)) {
            SetError(error, write
                ? "guest memory region is not writable"
                : "guest memory region is not readable");
            return false;
        }

        const auto offset = currentAddress - region.baseAddress;
        const auto available = region.size - offset;
        const auto chunk = (std::min)(remaining, static_cast<std::size_t>(available));
        if (write) {
            std::memcpy(storage_[index].data() + offset, bytes, chunk);
        } else {
            std::memcpy(bytes, storage_[index].data() + offset, chunk);
        }

        bytes += chunk;
        remaining -= chunk;
        currentAddress += chunk;
    }

    return true;
}

} // namespace ChonkyStation4::Core::Memory
