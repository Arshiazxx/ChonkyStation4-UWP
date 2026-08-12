#pragma once

#include "Core/Common/VirtualAddress.hpp"
#include "Core/Loader/Dependencies/Dependency.hpp"
#include "Core/Loader/Elf64Loader.hpp"
#include "Core/Loader/Relocations/Relocation.hpp"
#include "Core/Loader/Symbols/Symbol.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace ChonkyStation4::Core::Loader {

enum class ModuleKind {
    MainExecutable,
    SharedObject,
};

enum class ModuleLoadState {
    Unloaded,
    Loading,
    Loaded,
    Failed,
    Unloading,
};

class LoadedModule final {
public:
    LoadedModule() = default;
    explicit LoadedModule(std::string name, ModuleKind kind = ModuleKind::SharedObject);

    static bool FromElfReport(
        const std::string& name,
        const ElfLoadReport& report,
        LoadedModule& module,
        std::string* error = nullptr);

    const std::string& Name() const noexcept;
    const std::string& FilePath() const noexcept;
    ModuleKind Kind() const noexcept;
    ModuleLoadState State() const noexcept;
    bool IsLoaded() const noexcept;
    GuestVirtualAddress BaseAddress() const noexcept;
    std::uint64_t Size() const noexcept;
    GuestVirtualAddress EntryPoint() const noexcept;
    bool ResolveEntryPoint(GuestVirtualAddress& entryPoint) const noexcept;

    const std::vector<ElfLoadSegment>& Segments() const noexcept;
    const std::vector<SymbolInfo>& ExportedSymbols() const noexcept;
    const std::vector<SymbolInfo>& ImportedSymbols() const noexcept;
    // Kept as the M11 compatibility view; it returns exported symbols.
    const std::vector<SymbolInfo>& Symbols() const noexcept;
    const std::vector<ModuleDependency>& Dependencies() const noexcept;
    const std::vector<RelocationRecord>& Relocations() const noexcept;

    void AddSymbol(SymbolInfo symbol);
    void AddExportedSymbol(SymbolInfo symbol);
    void AddImportedSymbol(SymbolInfo symbol);
    void AddDependency(ModuleDependency dependency);
    void AddRelocation(RelocationRecord relocation);

    const SymbolInfo* FindExportedSymbol(const std::string& name) const noexcept;
    SymbolInfo* FindImportedSymbol(const std::string& name) noexcept;
    bool MarkImportedSymbolResolved(
        const std::string& name,
        GuestVirtualAddress address) noexcept;
    bool MarkDependencyResolved(const std::string& name, bool resolved) noexcept;

private:
    std::string name_;
    std::string filePath_;
    ModuleKind kind_ = ModuleKind::SharedObject;
    ModuleLoadState state_ = ModuleLoadState::Unloaded;
    GuestVirtualAddress baseAddress_ = InvalidGuestVirtualAddress;
    std::uint64_t size_ = 0;
    GuestVirtualAddress entryPoint_ = InvalidGuestVirtualAddress;
    std::vector<ElfLoadSegment> segments_;
    std::vector<SymbolInfo> exportedSymbols_;
    std::vector<SymbolInfo> importedSymbols_;
    std::vector<ModuleDependency> dependencies_;
    std::vector<RelocationRecord> relocations_;
};

} // namespace ChonkyStation4::Core::Loader
