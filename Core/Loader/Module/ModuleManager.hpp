#pragma once

#include "Core/Loader/Dependencies/DependencyResolver.hpp"
#include "Core/Loader/Elf64Loader.hpp"
#include "Core/Loader/Module/ModuleRegistry.hpp"
#include "Core/Loader/Relocations/Relocation.hpp"
#include "Core/Loader/Symbols/SymbolResolver.hpp"
#include "Core/Memory/GuestMemory.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace ChonkyStation4::Core::Loader {

struct RelocationProcessingReport {
    bool success = false;
    std::size_t processedCount = 0;
    std::size_t appliedCount = 0;
    std::vector<RelocationResult> results;
    std::string log;
    std::string error;
};

class ModuleManager final {
public:
    explicit ModuleManager(Memory::GuestMemory& memory) noexcept;

    ModuleRegistry& Registry() noexcept;
    const ModuleRegistry& Registry() const noexcept;
    Memory::GuestMemory& AddressSpace() noexcept;

    bool RegisterModule(const LoadedModule& module, std::string* error = nullptr);
    bool RegisterMainModule(const LoadedModule& module, std::string* error = nullptr);

    bool LoadModuleFromFile(
        const std::string& name,
        const std::string& path,
        ModuleKind kind = ModuleKind::SharedObject,
        std::string* error = nullptr);
    bool LoadModuleFromBytes(
        const std::string& name,
        const std::string& sourcePath,
        const std::vector<std::uint8_t>& bytes,
        ModuleKind kind = ModuleKind::SharedObject,
        std::string* error = nullptr);

    bool UnloadModule(const std::string& name, std::string* error = nullptr);
    LoadedModule* FindModule(const std::string& name) noexcept;
    const LoadedModule* FindModule(const std::string& name) const noexcept;
    LoadedModule* MainModule() noexcept;
    const LoadedModule* MainModule() const noexcept;

    DependencyResolutionReport ResolveDependencies(const std::string& moduleName);
    ImportResolutionReport ResolveImports(const std::string& moduleName);
    SymbolResolutionResult ResolveSymbol(const std::string& symbolName) const;
    RelocationProcessingReport ApplyRelocations(const std::string& moduleName);

private:
    bool RegisterLoadedModule(
        const LoadedModule& module,
        const ElfLoadReport& report,
        std::string* error);

    Memory::GuestMemory* memory_ = nullptr;
    ModuleRegistry registry_;
};

} // namespace ChonkyStation4::Core::Loader
