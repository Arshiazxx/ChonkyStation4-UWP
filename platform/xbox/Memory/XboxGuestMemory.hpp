#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

namespace ChonkyStation4::Xbox::Memory {

struct GuestMemoryLayout final {
    static constexpr std::uintptr_t ReservationBase = 0x0000000080000000ull;
    static constexpr std::uint64_t ReservationSize = 2048ull * 1024ull * 1024ull * 1024ull;
    static constexpr std::uint64_t AllocationSearchSize = 2000ull * 1024ull * 1024ull * 1024ull;
    static constexpr std::uintptr_t SystemMappingArea = 0x0000001000000000ull;
    static constexpr std::size_t GuestPageSize = 16ull * 1024ull;
};

enum class DataProtection {
    NoAccess,
    ReadOnly,
    ReadWrite,
};

struct MemoryOperationResult final {
    bool success = false;
    unsigned long nativeError = 0;
    std::string detail;
};

class XboxGuestMemory final {
public:
    XboxGuestMemory() = default;
    ~XboxGuestMemory();

    XboxGuestMemory(const XboxGuestMemory&) = delete;
    XboxGuestMemory& operator=(const XboxGuestMemory&) = delete;

    MemoryOperationResult ReserveUpstreamAddressSpace();
    MemoryOperationResult Commit(std::uint64_t offset, std::size_t size, void** address = nullptr);
    MemoryOperationResult Protect(std::uint64_t offset, std::size_t size, DataProtection protection);
    MemoryOperationResult Decommit(std::uint64_t offset, std::size_t size);
    MemoryOperationResult Release();

    bool IsReserved() const noexcept { return reservation_ != nullptr; }
    void* Base() const noexcept { return reservation_; }

private:
    bool IsRangeInside(std::uint64_t offset, std::size_t size) const noexcept;
    void* reservation_ = nullptr;
};

struct GuestMemoryDiagnosticReport final {
    bool exactReservation = false;
    bool reservationQuery = false;
    bool commit = false;
    bool writeRead = false;
    bool readOnlyProtection = false;
    bool readWriteRestore = false;
    bool decommit = false;
    bool sharedMapping = false;
    bool release = false;
    std::uint32_t hostPageSize = 0;
    std::uint32_t allocationGranularity = 0;
    unsigned long lastError = 0;
    std::string detail;

    bool CoreDataMemoryRequirementsPassed() const noexcept;
    std::string ToJson() const;
    std::wstring ToSummary() const;
};

GuestMemoryDiagnosticReport RunGuestMemoryDiagnostic();

} // namespace ChonkyStation4::Xbox::Memory
