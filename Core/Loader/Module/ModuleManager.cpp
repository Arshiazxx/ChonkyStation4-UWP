#include "ModuleManager.hpp"

#include <sstream>

namespace ChonkyStation4::Core::Loader {

namespace {

void SetError(std::string* error, const std::string& message) {
    if (error != nullptr) {
        *error = message;
    }
}

void UnmapReport(Memory::GuestMemory& memory, const ElfLoadReport& report) {
    for (const auto& segment : report.loadableSegments) {
        if (segment.memorySize != 0) {
            memory.Unmap(segment.virtualAddress);
        }
    }
}

} // namespace

ModuleManager::ModuleManager(Memory::GuestMemory& memory) noexcept
    : memory_(&memory) {}

ModuleRegistry& ModuleManager::Registry() noexcept {
    return registry_;
}

const ModuleRegistry& ModuleManager::Registry() const noexcept {
    return registry_;
}

Memory::GuestMemory& ModuleManager::AddressSpace() noexcept {
    return *memory_;
}

bool ModuleManager::RegisterModule(const LoadedModule& module, std::string* error) {
    if (module.Kind() == ModuleKind::MainExecutable) {
        return RegisterMainModule(module, error);
    }
    return registry_.Register(module, error);
}

bool ModuleManager::RegisterMainModule(const LoadedModule& module, std::string* error) {
    return registry_.RegisterMain(module, error);
}

bool ModuleManager::RegisterLoadedModule(
    const LoadedModule& module,
    const ElfLoadReport& report,
    std::string* error) {
    if (!RegisterModule(module, error)) {
        UnmapReport(*memory_, report);
        return false;
    }
    return true;
}

bool ModuleManager::LoadModuleFromFile(
    const std::string& name,
    const std::string& path,
    ModuleKind kind,
    std::string* error) {
    if (registry_.Find(name) != nullptr) {
        SetError(error, "module is already registered: " + name);
        return false;
    }
    Elf64Loader loader;
    ElfLoadReport report;
    if (!loader.LoadIntoMemory(path, *memory_, &report)) {
        SetError(error, report.error);
        return false;
    }
    LoadedModule module(name, kind);
    if (!LoadedModule::FromElfReport(name, report, module, error)) {
        UnmapReport(*memory_, report);
        return false;
    }
    return RegisterLoadedModule(module, report, error);
}

bool ModuleManager::LoadModuleFromBytes(
    const std::string& name,
    const std::string& sourcePath,
    const std::vector<std::uint8_t>& bytes,
    ModuleKind kind,
    std::string* error) {
    if (registry_.Find(name) != nullptr) {
        SetError(error, "module is already registered: " + name);
        return false;
    }
    Elf64Loader loader;
    ElfLoadReport report;
    if (!loader.LoadBytesIntoMemory(sourcePath, bytes, *memory_, &report)) {
        SetError(error, report.error);
        return false;
    }
    LoadedModule module(name, kind);
    if (!LoadedModule::FromElfReport(name, report, module, error)) {
        UnmapReport(*memory_, report);
        return false;
    }
    return RegisterLoadedModule(module, report, error);
}

bool ModuleManager::UnloadModule(const std::string& name, std::string* error) {
    auto* module = registry_.Find(name);
    if (module == nullptr) {
        SetError(error, "module is not registered: " + name);
        return false;
    }
    for (const auto& segment : module->Segments()) {
        if (segment.memorySize == 0) {
            continue;
        }
        std::string unmapError;
        if (!memory_->Unmap(segment.virtualAddress, &unmapError)) {
            SetError(error, "unable to unload module segment: " + unmapError);
            return false;
        }
    }
    return registry_.Remove(name, error);
}

LoadedModule* ModuleManager::FindModule(const std::string& name) noexcept {
    return registry_.Find(name);
}

const LoadedModule* ModuleManager::FindModule(const std::string& name) const noexcept {
    return registry_.Find(name);
}

LoadedModule* ModuleManager::MainModule() noexcept {
    return registry_.MainModule();
}

const LoadedModule* ModuleManager::MainModule() const noexcept {
    return registry_.MainModule();
}

DependencyResolutionReport ModuleManager::ResolveDependencies(const std::string& moduleName) {
    DependencyResolver resolver(registry_);
    auto report = resolver.Resolve(moduleName);
    if (report.success) {
        if (auto* module = registry_.Find(moduleName); module != nullptr) {
            for (const auto& dependency : module->Dependencies()) {
                module->MarkDependencyResolved(dependency.name, true);
            }
        }
    }
    return report;
}

ImportResolutionReport ModuleManager::ResolveImports(const std::string& moduleName) {
    ImportResolutionReport report;
    auto* module = registry_.Find(moduleName);
    if (module == nullptr) {
        report.error = "module is not registered: " + moduleName;
        return report;
    }
    SymbolResolver resolver(registry_);
    return resolver.ResolveImports(*module);
}

SymbolResolutionResult ModuleManager::ResolveSymbol(const std::string& symbolName) const {
    SymbolResolver resolver(registry_);
    return resolver.ResolveExport(symbolName);
}

RelocationProcessingReport ModuleManager::ApplyRelocations(const std::string& moduleName) {
    RelocationProcessingReport report;
    auto* module = registry_.Find(moduleName);
    if (module == nullptr) {
        report.error = "module is not registered: " + moduleName;
        return report;
    }

    SymbolResolver symbols(registry_);
    RelocationContext context{
        *memory_, module->BaseAddress(), module->Size(), &symbols};
    SafeRelocationResolver resolver;
    std::ostringstream log;
    for (const auto& relocation : module->Relocations()) {
        const auto result = resolver.Resolve(relocation, context);
        report.results.push_back(result);
        ++report.processedCount;
        if (result.Succeeded()) {
            ++report.appliedCount;
        } else if (report.error.empty()) {
            report.error = result.error;
        }
        log << result.log << "\n";
    }
    report.success = report.error.empty();
    report.log = log.str();
    return report;
}

} // namespace ChonkyStation4::Core::Loader
