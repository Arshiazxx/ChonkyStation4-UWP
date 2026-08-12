#include "XboxGuestMemory.hpp"

#define NOMINMAX
#include <windows.h>

#include <algorithm>
#include <cstring>
#include <iomanip>
#include <sstream>
#include <utility>

namespace ChonkyStation4::Xbox::Memory {
namespace {

static_assert(sizeof(void*) == 8, "The upstream guest address-space model requires a 64-bit host process.");

std::string Hex(std::uint64_t value) {
    std::ostringstream out;
    out << "0x" << std::hex << std::uppercase << value;
    return out.str();
}

std::string EscapeJson(const std::string& value) {
    std::ostringstream out;
    for (unsigned char c : value) {
        switch (c) {
        case '\\': out << "\\\\"; break;
        case '"': out << "\\\""; break;
        case '\n': out << "\\n"; break;
        case '\r': out << "\\r"; break;
        case '\t': out << "\\t"; break;
        default:
            if (c < 0x20) {
                const char hex[] = "0123456789abcdef";
                out << "\\u00" << hex[(c >> 4) & 0xf] << hex[c & 0xf];
            } else {
                out << static_cast<char>(c);
            }
        }
    }
    return out.str();
}

DWORD ToNativeProtection(DataProtection protection) {
    switch (protection) {
    case DataProtection::NoAccess: return PAGE_NOACCESS;
    case DataProtection::ReadOnly: return PAGE_READONLY;
    case DataProtection::ReadWrite: return PAGE_READWRITE;
    }
    return PAGE_NOACCESS;
}

MemoryOperationResult Failure(const char* operation) {
    MemoryOperationResult result;
    result.nativeError = GetLastError();
    result.detail = std::string(operation) + " failed; GetLastError=" + std::to_string(result.nativeError);
    return result;
}

MemoryOperationResult Success(std::string detail) {
    MemoryOperationResult result;
    result.success = true;
    result.detail = std::move(detail);
    return result;
}

} // namespace

XboxGuestMemory::~XboxGuestMemory() {
    if (reservation_) {
        VirtualFree(reservation_, 0, MEM_RELEASE);
        reservation_ = nullptr;
    }
}

bool XboxGuestMemory::IsRangeInside(std::uint64_t offset, std::size_t size) const noexcept {
    if (!reservation_ || size == 0 || offset >= GuestMemoryLayout::ReservationSize) return false;
    const auto remaining = GuestMemoryLayout::ReservationSize - offset;
    return static_cast<std::uint64_t>(size) <= remaining;
}

MemoryOperationResult XboxGuestMemory::ReserveUpstreamAddressSpace() {
    if (reservation_) return Success("upstream guest address space already reserved");

    auto* requested = reinterpret_cast<void*>(GuestMemoryLayout::ReservationBase);
    void* reservation = VirtualAllocFromApp(
        requested,
        static_cast<SIZE_T>(GuestMemoryLayout::ReservationSize),
        MEM_RESERVE,
        PAGE_NOACCESS);

    if (!reservation) return Failure("VirtualAllocFromApp(MEM_RESERVE)");
    if (reservation != requested) {
        VirtualFree(reservation, 0, MEM_RELEASE);
        SetLastError(ERROR_INVALID_ADDRESS);
        return Failure("VirtualAllocFromApp exact-base verification");
    }

    reservation_ = reservation;
    return Success("reserved " + std::to_string(GuestMemoryLayout::ReservationSize) +
                   " bytes at " + Hex(GuestMemoryLayout::ReservationBase));
}

MemoryOperationResult XboxGuestMemory::Commit(std::uint64_t offset, std::size_t size, void** address) {
    if (!IsRangeInside(offset, size)) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return Failure("Commit range validation");
    }

    auto* target = reinterpret_cast<void*>(GuestMemoryLayout::ReservationBase + offset);
    void* committed = VirtualAllocFromApp(target, size, MEM_COMMIT, PAGE_READWRITE);
    if (!committed || committed != target) return Failure("VirtualAllocFromApp(MEM_COMMIT)");
    if (address) *address = committed;
    return Success("committed data pages at " + Hex(reinterpret_cast<std::uintptr_t>(committed)));
}

MemoryOperationResult XboxGuestMemory::Protect(std::uint64_t offset, std::size_t size, DataProtection protection) {
    if (!IsRangeInside(offset, size)) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return Failure("Protect range validation");
    }

    DWORD oldProtection = 0;
    auto* target = reinterpret_cast<void*>(GuestMemoryLayout::ReservationBase + offset);
    if (!VirtualProtectFromApp(target, size, ToNativeProtection(protection), &oldProtection)) {
        return Failure("VirtualProtectFromApp");
    }
    return Success("data-page protection changed; old protection=" + std::to_string(oldProtection));
}

MemoryOperationResult XboxGuestMemory::Decommit(std::uint64_t offset, std::size_t size) {
    if (!IsRangeInside(offset, size)) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return Failure("Decommit range validation");
    }

    auto* target = reinterpret_cast<void*>(GuestMemoryLayout::ReservationBase + offset);
    if (!VirtualFree(target, size, MEM_DECOMMIT)) return Failure("VirtualFree(MEM_DECOMMIT)");
    return Success("decommitted data pages while retaining the reservation");
}

MemoryOperationResult XboxGuestMemory::Release() {
    if (!reservation_) return Success("no guest reservation was active");
    if (!VirtualFree(reservation_, 0, MEM_RELEASE)) return Failure("VirtualFree(MEM_RELEASE)");
    reservation_ = nullptr;
    return Success("released upstream guest address-space reservation");
}

bool GuestMemoryDiagnosticReport::CoreDataMemoryRequirementsPassed() const noexcept {
    return exactReservation && reservationQuery && commit && writeRead && readOnlyProtection &&
           readWriteRestore && decommit && release;
}

std::string GuestMemoryDiagnosticReport::ToJson() const {
    std::ostringstream out;
    out << "{\n"
        << "  \"source_commit\": \"310269290a3c256f5911d4bc7e441489bffffbf6\",\n"
        << "  \"guest_base\": \"0x0000000080000000\",\n"
        << "  \"reservation_bytes\": " << GuestMemoryLayout::ReservationSize << ",\n"
        << "  \"allocation_search_bytes\": " << GuestMemoryLayout::AllocationSearchSize << ",\n"
        << "  \"guest_page_bytes\": " << GuestMemoryLayout::GuestPageSize << ",\n"
        << "  \"host_page_bytes\": " << hostPageSize << ",\n"
        << "  \"allocation_granularity_bytes\": " << allocationGranularity << ",\n"
        << "  \"exact_reservation\": " << (exactReservation ? "true" : "false") << ",\n"
        << "  \"reservation_query\": " << (reservationQuery ? "true" : "false") << ",\n"
        << "  \"commit\": " << (commit ? "true" : "false") << ",\n"
        << "  \"write_read\": " << (writeRead ? "true" : "false") << ",\n"
        << "  \"read_only_protection\": " << (readOnlyProtection ? "true" : "false") << ",\n"
        << "  \"read_write_restore\": " << (readWriteRestore ? "true" : "false") << ",\n"
        << "  \"decommit\": " << (decommit ? "true" : "false") << ",\n"
        << "  \"shared_mapping\": " << (sharedMapping ? "true" : "false") << ",\n"
        << "  \"release\": " << (release ? "true" : "false") << ",\n"
        << "  \"core_data_memory_requirements_passed\": " << (CoreDataMemoryRequirementsPassed() ? "true" : "false") << ",\n"
        << "  \"last_error\": " << lastError << ",\n"
        << "  \"detail\": \"" << EscapeJson(detail) << "\",\n"
        << "  \"executable_guest_memory\": \"NOT_TESTED_M7\",\n"
        << "  \"guest_execution\": \"NOT_TESTED_M7\"\n"
        << "}\n";
    return out.str();
}

std::wstring GuestMemoryDiagnosticReport::ToSummary() const {
    std::wostringstream out;
    out << L"Upstream guest base: 0x0000000080000000\n"
        << L"Upstream reservation: 2048 GiB\n"
        << L"Guest page contract: 16 KiB\n"
        << L"Host page: " << hostPageSize << L" bytes\n"
        << L"Allocation granularity: " << allocationGranularity << L" bytes\n\n"
        << L"Exact reservation: " << (exactReservation ? L"PASS" : L"FAIL") << L"\n"
        << L"VirtualQuery reservation: " << (reservationQuery ? L"PASS" : L"FAIL") << L"\n"
        << L"Commit: " << (commit ? L"PASS" : L"FAIL") << L"\n"
        << L"Write/read: " << (writeRead ? L"PASS" : L"FAIL") << L"\n"
        << L"Read-only protection: " << (readOnlyProtection ? L"PASS" : L"FAIL") << L"\n"
        << L"Read/write restore: " << (readWriteRestore ? L"PASS" : L"FAIL") << L"\n"
        << L"Decommit: " << (decommit ? L"PASS" : L"FAIL") << L"\n"
        << L"Pagefile-backed shared mapping: " << (sharedMapping ? L"PASS" : L"FAIL") << L"\n"
        << L"Release: " << (release ? L"PASS" : L"FAIL") << L"\n\n"
        << L"Executable guest pages: NOT TESTED (M7)\n"
        << L"Guest execution: NOT TESTED (M7)\n";
    if (!detail.empty()) out << L"Detail: " << std::wstring(detail.begin(), detail.end()) << L"\n";
    return out.str();
}

GuestMemoryDiagnosticReport RunGuestMemoryDiagnostic() {
    GuestMemoryDiagnosticReport report;
    SYSTEM_INFO info{};
    GetSystemInfo(&info);
    report.hostPageSize = info.dwPageSize;
    report.allocationGranularity = info.dwAllocationGranularity;

    XboxGuestMemory memory;
    auto reserve = memory.ReserveUpstreamAddressSpace();
    report.exactReservation = reserve.success;
    if (!reserve.success) {
        report.lastError = reserve.nativeError;
        report.detail = reserve.detail;
        return report;
    }

    MEMORY_BASIC_INFORMATION mbi{};
    const SIZE_T queried = VirtualQuery(memory.Base(), &mbi, sizeof(mbi));
    report.reservationQuery = queried == sizeof(mbi) &&
        mbi.AllocationBase == memory.Base() && mbi.State == MEM_RESERVE;
    if (!report.reservationQuery) report.lastError = GetLastError();

    const std::size_t commitSize = std::max<std::size_t>(GuestMemoryLayout::GuestPageSize, info.dwPageSize);
    void* committed = nullptr;
    auto commit = memory.Commit(0, commitSize, &committed);
    report.commit = commit.success;
    if (commit.success) {
        auto* bytes = static_cast<unsigned char*>(committed);
        bytes[0] = 0x43;
        bytes[commitSize - 1] = 0x34;
        report.writeRead = bytes[0] == 0x43 && bytes[commitSize - 1] == 0x34;

        const auto readOnly = memory.Protect(0, commitSize, DataProtection::ReadOnly);
        report.readOnlyProtection = readOnly.success;
        if (readOnly.success) {
            const auto readWrite = memory.Protect(0, commitSize, DataProtection::ReadWrite);
            report.readWriteRestore = readWrite.success;
        }

        const auto decommit = memory.Decommit(0, commitSize);
        report.decommit = decommit.success;
    } else {
        report.lastError = commit.nativeError;
        report.detail = commit.detail;
    }

    constexpr std::uint64_t mappingSize = 64ull * 1024ull;
    HANDLE mapping = CreateFileMappingFromApp(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, mappingSize, nullptr);
    if (mapping) {
        void* view = MapViewOfFileFromApp(mapping, FILE_MAP_WRITE, 0, static_cast<SIZE_T>(mappingSize));
        if (view) {
            auto* bytes = static_cast<unsigned char*>(view);
            bytes[0] = 0x5a;
            bytes[mappingSize - 1] = 0xa5;
            report.sharedMapping = bytes[0] == 0x5a && bytes[mappingSize - 1] == 0xa5;
            UnmapViewOfFile(view);
        }
        CloseHandle(mapping);
    }

    const auto release = memory.Release();
    report.release = release.success;
    if (!release.success) {
        report.lastError = release.nativeError;
        report.detail = release.detail;
    }

    return report;
}

} // namespace ChonkyStation4::Xbox::Memory
