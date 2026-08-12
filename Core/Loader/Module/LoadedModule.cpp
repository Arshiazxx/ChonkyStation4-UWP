#include "LoadedModule.hpp"

#include <algorithm>
#include <limits>
#include <utility>

namespace ChonkyStation4::Core::Loader {

namespace {

void SetError(std::string* error, const char* message) {
    if (error != nullptr) {
        *error = message;
    }
}

bool AddOverflows(std::uint64_t base, std::uint64_t size) {
    return size > (std::numeric_limits<std::uint64_t>::max)() - base;
}

} // namespace

LoadedModule::LoadedModule(std::string name, ModuleKind kind)
    : name_(std::move(name)), kind_(kind) {}

bool LoadedModule::FromElfReport(
    const std::string& name,
    const ElfLoadReport& report,
    LoadedModule& module,
    std::string* error) {
    if (!report.success) {
        SetError(error, "cannot create a module from a failed ELF load report");
        return false;
    }
    if (report.loadableSegments.empty()) {
        SetError(error, "ELF image does not contain a loadable segment");
        return false;
    }

    GuestVirtualAddress minimumAddress = (std::numeric_limits<GuestVirtualAddress>::max)();
    GuestVirtualAddress maximumAddress = 0;
    for (const auto& segment : report.loadableSegments) {
        if (segment.memorySize == 0 || AddOverflows(segment.virtualAddress, segment.memorySize)) {
            SetError(error, "ELF module segment has an invalid address range");
            return false;
        }
        minimumAddress = (std::min)(minimumAddress, segment.virtualAddress);
        maximumAddress = (std::max)(maximumAddress, segment.virtualAddress + segment.memorySize);
    }
    if (minimumAddress == InvalidGuestVirtualAddress || maximumAddress <= minimumAddress) {
        SetError(error, "ELF module has no usable mapped address range");
        return false;
    }

    module.name_ = name;
    module.filePath_ = report.filePath;
    module.state_ = ModuleLoadState::Loaded;
    module.baseAddress_ = minimumAddress;
    module.size_ = maximumAddress - minimumAddress;
    module.entryPoint_ = report.entryPoint;
    module.segments_ = report.loadableSegments;
    module.exportedSymbols_.clear();
    module.importedSymbols_.clear();
    module.dependencies_.clear();
    module.relocations_.clear();
    return true;
}

const std::string& LoadedModule::Name() const noexcept {
    return name_;
}

const std::string& LoadedModule::FilePath() const noexcept {
    return filePath_;
}

ModuleKind LoadedModule::Kind() const noexcept {
    return kind_;
}

ModuleLoadState LoadedModule::State() const noexcept {
    return state_;
}

bool LoadedModule::IsLoaded() const noexcept {
    return state_ == ModuleLoadState::Loaded;
}

GuestVirtualAddress LoadedModule::BaseAddress() const noexcept {
    return baseAddress_;
}

std::uint64_t LoadedModule::Size() const noexcept {
    return size_;
}

GuestVirtualAddress LoadedModule::EntryPoint() const noexcept {
    return entryPoint_;
}

bool LoadedModule::ResolveEntryPoint(GuestVirtualAddress& entryPoint) const noexcept {
    if (!IsLoaded() || entryPoint_ == InvalidGuestVirtualAddress) {
        return false;
    }
    entryPoint = entryPoint_;
    return true;
}

const std::vector<ElfLoadSegment>& LoadedModule::Segments() const noexcept {
    return segments_;
}

const std::vector<SymbolInfo>& LoadedModule::Symbols() const noexcept {
    return exportedSymbols_;
}

const std::vector<SymbolInfo>& LoadedModule::ExportedSymbols() const noexcept {
    return exportedSymbols_;
}

const std::vector<SymbolInfo>& LoadedModule::ImportedSymbols() const noexcept {
    return importedSymbols_;
}

const std::vector<ModuleDependency>& LoadedModule::Dependencies() const noexcept {
    return dependencies_;
}

const std::vector<RelocationRecord>& LoadedModule::Relocations() const noexcept {
    return relocations_;
}

void LoadedModule::AddSymbol(SymbolInfo symbol) {
    AddExportedSymbol(std::move(symbol));
}

void LoadedModule::AddExportedSymbol(SymbolInfo symbol) {
    symbol.defined = true;
    exportedSymbols_.push_back(std::move(symbol));
}

void LoadedModule::AddImportedSymbol(SymbolInfo symbol) {
    symbol.defined = false;
    importedSymbols_.push_back(std::move(symbol));
}

void LoadedModule::AddDependency(ModuleDependency dependency) {
    dependencies_.push_back(std::move(dependency));
}

void LoadedModule::AddRelocation(RelocationRecord relocation) {
    relocations_.push_back(std::move(relocation));
}

const SymbolInfo* LoadedModule::FindExportedSymbol(const std::string& name) const noexcept {
    for (const auto& symbol : exportedSymbols_) {
        if (symbol.name == name) {
            return &symbol;
        }
    }
    return nullptr;
}

SymbolInfo* LoadedModule::FindImportedSymbol(const std::string& name) noexcept {
    for (auto& symbol : importedSymbols_) {
        if (symbol.name == name) {
            return &symbol;
        }
    }
    return nullptr;
}

bool LoadedModule::MarkImportedSymbolResolved(
    const std::string& name,
    GuestVirtualAddress address) noexcept {
    auto* symbol = FindImportedSymbol(name);
    if (symbol == nullptr) {
        return false;
    }
    symbol->resolved = true;
    symbol->resolvedAddress = address;
    return true;
}

bool LoadedModule::MarkDependencyResolved(
    const std::string& name,
    bool resolved) noexcept {
    for (auto& dependency : dependencies_) {
        if (dependency.name == name) {
            dependency.resolved = resolved;
            return true;
        }
    }
    return false;
}

} // namespace ChonkyStation4::Core::Loader
