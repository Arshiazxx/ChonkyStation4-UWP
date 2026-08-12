#pragma once

#include "Core/Common/VirtualAddress.hpp"

#include <cstdint>
#include <string>

namespace ChonkyStation4::Core::Loader {

enum class SymbolBinding : std::uint8_t {
    Local,
    Global,
    Weak,
    Unknown,
};

enum class SymbolType : std::uint8_t {
    NoType,
    Object,
    Function,
    Section,
    File,
    Unknown,
};

enum class SymbolValueKind : std::uint8_t {
    RelativeToModule,
    Absolute,
};

struct SymbolInfo {
    std::string name;
    std::uint64_t value = 0;
    std::uint64_t size = 0;
    SymbolBinding binding = SymbolBinding::Unknown;
    SymbolType type = SymbolType::Unknown;
    SymbolValueKind valueKind = SymbolValueKind::RelativeToModule;
    bool defined = false;
    bool resolved = false;
    GuestVirtualAddress resolvedAddress = InvalidGuestVirtualAddress;
};

} // namespace ChonkyStation4::Core::Loader
