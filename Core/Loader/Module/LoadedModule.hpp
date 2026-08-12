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
    bool IsLoaded() const noexcept;
    GuestVirtualAddress BaseAddress() const noexcept;
    std::uint64_t Size() const noexcept;
    GuestVirtualAddress EntryPoint() const noexcept;
    bool ResolveEntryPoint(GuestVirtualAddress& entryPoint) const noexcept;

    const std::vector<ElfLoadSegment>& Segments() const noexcept;
    const std::vector<SymbolInfo>& Symbols() const noexcept;
    const std::vector<ModuleDependency>& Dependencies() const noexcept;
    const std::vector<RelocationRecord>& Relocations() const noexcept;

    void AddSymbol(SymbolInfo symbol);
    void AddDependency(ModuleDependency dependency);
    void AddRelocation(RelocationRecord relocation);

private:
    std::string name_;
    std::string filePath_;
    ModuleKind kind_ = ModuleKind::SharedObject;
    bool loaded_ = false;
    GuestVirtualAddress baseAddress_ = InvalidGuestVirtualAddress;
    std::uint64_t size_ = 0;
    GuestVirtualAddress entryPoint_ = InvalidGuestVirtualAddress;
    std::vector<ElfLoadSegment> segments_;
    std::vector<SymbolInfo> symbols_;
    std::vector<ModuleDependency> dependencies_;
    std::vector<RelocationRecord> relocations_;
};

} // namespace ChonkyStation4::Core::Loader
