#pragma once

#include "Core/Loader/Module/ModuleRegistry.hpp"
#include "Core/Loader/Relocations/Relocation.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace ChonkyStation4::Core::Loader {

enum class SymbolResolutionStatus {
    Found,
    Missing,
    Invalid,
};

struct SymbolResolutionResult {
    SymbolResolutionStatus status = SymbolResolutionStatus::Invalid;
    std::string symbolName;
    std::string moduleName;
    GuestVirtualAddress address = InvalidGuestVirtualAddress;
    SymbolInfo symbol{};
    std::string log;
    std::string error;

    bool Found() const noexcept {
        return status == SymbolResolutionStatus::Found;
    }
};

struct ImportResolutionReport {
    bool success = false;
    std::size_t resolvedCount = 0;
    std::vector<std::string> missingSymbols;
    std::string log;
    std::string error;
};

class SymbolResolver final : public IRelocationSymbolResolver {
public:
    explicit SymbolResolver(const ModuleRegistry& registry) noexcept;

    SymbolResolutionResult ResolveExport(const std::string& symbolName) const;
    ImportResolutionReport ResolveImports(LoadedModule& module) const;

    bool ResolveSymbol(
        const std::string& symbolName,
        GuestVirtualAddress& address,
        std::string* error = nullptr) const override;

private:
    const ModuleRegistry* registry_ = nullptr;
};

} // namespace ChonkyStation4::Core::Loader
