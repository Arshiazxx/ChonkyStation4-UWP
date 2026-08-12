#pragma once

#include "Core/Common/VirtualAddress.hpp"

#include <cstdint>
#include <string>

namespace ChonkyStation4::Core::Memory {
class GuestMemory;
}

namespace ChonkyStation4::Core::Loader {

enum class RelocationType : std::uint32_t {
    None = 0,
    Absolute64 = 1,
    GlobDat64 = 6,
    JumpSlot64 = 7,
    Relative64 = 8,
    Unknown = 0xffffffffu,
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
    MissingSymbol,
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
    const class IRelocationSymbolResolver* symbolResolver = nullptr;
};

class IRelocationSymbolResolver {
public:
    virtual ~IRelocationSymbolResolver() = default;

    virtual bool ResolveSymbol(
        const std::string& symbolName,
        GuestVirtualAddress& address,
        std::string* error = nullptr) const = 0;
};

class IRelocationResolver {
public:
    virtual ~IRelocationResolver() = default;

    virtual RelocationResult Resolve(
        const RelocationRecord& relocation,
        RelocationContext& context) const = 0;
};

// The resolver supports bounded relative relocations and symbol-based
// absolute/GOT/PLT-shaped writes. Unsupported records are never written.
class SafeRelocationResolver final : public IRelocationResolver {
public:
    RelocationResult Resolve(
        const RelocationRecord& relocation,
        RelocationContext& context) const override;
};

} // namespace ChonkyStation4::Core::Loader
