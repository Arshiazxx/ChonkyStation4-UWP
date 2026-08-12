#include "SymbolResolver.hpp"

#include <limits>
#include <sstream>

namespace ChonkyStation4::Core::Loader {

namespace {

bool AddOverflows(std::uint64_t base, std::uint64_t value) {
    return value > (std::numeric_limits<std::uint64_t>::max)() - base;
}

bool ResolveAddress(
    const LoadedModule& module,
    const SymbolInfo& symbol,
    GuestVirtualAddress& address,
    std::string& error) {
    if (symbol.valueKind == SymbolValueKind::Absolute) {
        address = symbol.value;
        return true;
    }
    if (AddOverflows(module.BaseAddress(), symbol.value)) {
        error = "symbol address overflows module base";
        return false;
    }
    address = module.BaseAddress() + symbol.value;
    return true;
}

} // namespace

SymbolResolver::SymbolResolver(const ModuleRegistry& registry) noexcept
    : registry_(&registry) {}

SymbolResolutionResult SymbolResolver::ResolveExport(const std::string& symbolName) const {
    SymbolResolutionResult result;
    result.symbolName = symbolName;
    if (registry_ == nullptr || symbolName.empty()) {
        result.status = SymbolResolutionStatus::Invalid;
        result.error = "symbol name is empty or registry is unavailable";
        result.log = result.error;
        return result;
    }

    for (const auto& module : registry_->Modules()) {
        const auto* symbol = module.FindExportedSymbol(symbolName);
        if (symbol == nullptr || !symbol->defined) {
            continue;
        }
        result.symbol = *symbol;
        result.moduleName = module.Name();
        std::string addressError;
        if (!ResolveAddress(module, *symbol, result.address, addressError)) {
            result.status = SymbolResolutionStatus::Invalid;
            result.error = addressError;
            result.log = result.error;
            return result;
        }
        result.status = SymbolResolutionStatus::Found;
        std::ostringstream log;
        log << "Export:\n" << symbolName << "\n\nResolution:\nModule: "
            << module.Name() << "\n\nResult: FOUND";
        result.log = log.str();
        return result;
    }

    result.status = SymbolResolutionStatus::Missing;
    result.error = "missing exported symbol: " + symbolName;
    result.log = result.error;
    return result;
}

ImportResolutionReport SymbolResolver::ResolveImports(LoadedModule& module) const {
    ImportResolutionReport report;
    std::ostringstream log;
    for (const auto& imported : module.ImportedSymbols()) {
        const auto resolution = ResolveExport(imported.name);
        if (resolution.Found()) {
            module.MarkImportedSymbolResolved(imported.name, resolution.address);
            ++report.resolvedCount;
            log << resolution.log << "\n\n";
            continue;
        }
        if (imported.binding == SymbolBinding::Weak) {
            module.MarkImportedSymbolResolved(imported.name, InvalidGuestVirtualAddress);
            log << "Import:\n" << imported.name << "\n\nResolution:\nWEAK / NOT FOUND\n\n";
            continue;
        }
        report.missingSymbols.push_back(imported.name);
        report.error = resolution.error;
        log << "Import:\n" << imported.name << "\n\nResolution:\nMISSING\n\n";
    }
    report.success = report.missingSymbols.empty();
    report.log = log.str();
    return report;
}

bool SymbolResolver::ResolveSymbol(
    const std::string& symbolName,
    GuestVirtualAddress& address,
    std::string* error) const {
    const auto result = ResolveExport(symbolName);
    if (!result.Found()) {
        if (error != nullptr) {
            *error = result.error;
        }
        return false;
    }
    address = result.address;
    return true;
}

} // namespace ChonkyStation4::Core::Loader
