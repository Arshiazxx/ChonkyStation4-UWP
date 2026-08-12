#pragma once

#include "Core/Common/VirtualAddress.hpp"

#include <cstdint>
#include <string>

namespace ChonkyStation4::Core::Memory {
class GuestMemory;
}

namespace ChonkyStation4::Core::Loader {

enum class RelocationType : std::uint32_t {
    Unknown = 0,
    Absolute64 = 1,
    GlobDat64 = 6,
    JumpSlot64 = 7,
    Relative64 = 8,
};

const char* RelocationTypeName(RelocationType type) noexcept;

struct RelocationRecord {
    RelocationType type = RelocationType::Unknown;
    std::uint64_t offset = 0;
    std::int64_t addend = 0;
    std::uint32_t symbolIndex = 0;
    std::string symbolName;
};

enum class RelocationStatus {
    Applied,
    NoOp,
    Unsupported,
    OutOfBounds,
    Invalid,
};

struct RelocationResult {
    RelocationStatus status = RelocationStatus::Invalid;
    std::uint64_t value = 0;
    std::string log;
    std::string error;

    bool Succeeded() const noexcept {
        return status == RelocationStatus::Applied || status == RelocationStatus::NoOp;
    }
};

struct RelocationContext {
    Memory::GuestMemory& memory;
    GuestVirtualAddress moduleBase = InvalidGuestVirtualAddress;
    std::uint64_t moduleSize = 0;
};

class IRelocationResolver {
public:
    virtual ~IRelocationResolver() = default;

    virtual RelocationResult Resolve(
        const RelocationRecord& relocation,
        RelocationContext& context) const = 0;
};

// M11 deliberately supports only the safe, non-symbolic R_X86_64_RELATIVE
// shape. Other relocations are reported and never written to guest memory.
class SafeRelocationResolver final : public IRelocationResolver {
public:
    RelocationResult Resolve(
        const RelocationRecord& relocation,
        RelocationContext& context) const override;
};

} // namespace ChonkyStation4::Core::Loader
