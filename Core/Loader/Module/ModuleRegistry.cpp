#include "ModuleRegistry.hpp"

namespace ChonkyStation4::Core::Loader {

namespace {

void SetError(std::string* error, const std::string& message) {
    if (error != nullptr) {
        *error = message;
    }
}

} // namespace

bool ModuleRegistry::Register(const LoadedModule& module, std::string* error) {
    if (!module.IsLoaded()) {
        SetError(error, "cannot register an unloaded module");
        return false;
    }
    if (module.Name().empty()) {
        SetError(error, "module name cannot be empty");
        return false;
    }
    if (Find(module.Name()) != nullptr) {
        SetError(error, "module is already registered: " + module.Name());
        return false;
    }
    modules_.push_back(module);
    return true;
}

bool ModuleRegistry::RegisterMain(const LoadedModule& module, std::string* error) {
    if (!module.IsLoaded()) {
        SetError(error, "cannot register an unloaded main module");
        return false;
    }
    if (!mainModuleName_.empty() && mainModuleName_ != module.Name()) {
        SetError(error, "a main module is already registered: " + mainModuleName_);
        return false;
    }
    if (auto* existing = Find(module.Name()); existing != nullptr) {
        *existing = module;
    } else if (!Register(module, error)) {
        return false;
    }
    mainModuleName_ = module.Name();
    return true;
}

bool ModuleRegistry::Remove(const std::string& name, std::string* error) {
    for (auto iterator = modules_.begin(); iterator != modules_.end(); ++iterator) {
        if (iterator->Name() == name) {
            modules_.erase(iterator);
            if (mainModuleName_ == name) {
                mainModuleName_.clear();
            }
            return true;
        }
    }
    SetError(error, "module is not registered: " + name);
    return false;
}

LoadedModule* ModuleRegistry::Find(const std::string& name) noexcept {
    for (auto& module : modules_) {
        if (module.Name() == name) {
            return &module;
        }
    }
    return nullptr;
}

const LoadedModule* ModuleRegistry::Find(const std::string& name) const noexcept {
    for (const auto& module : modules_) {
        if (module.Name() == name) {
            return &module;
        }
    }
    return nullptr;
}

LoadedModule* ModuleRegistry::FindByPath(const std::string& path) noexcept {
    for (auto& module : modules_) {
        if (module.FilePath() == path) {
            return &module;
        }
    }
    return nullptr;
}

const LoadedModule* ModuleRegistry::FindByPath(const std::string& path) const noexcept {
    for (const auto& module : modules_) {
        if (module.FilePath() == path) {
            return &module;
        }
    }
    return nullptr;
}

LoadedModule* ModuleRegistry::MainModule() noexcept {
    return mainModuleName_.empty() ? nullptr : Find(mainModuleName_);
}

const LoadedModule* ModuleRegistry::MainModule() const noexcept {
    return mainModuleName_.empty() ? nullptr : Find(mainModuleName_);
}

const std::vector<LoadedModule>& ModuleRegistry::Modules() const noexcept {
    return modules_;
}

std::size_t ModuleRegistry::Size() const noexcept {
    return modules_.size();
}

} // namespace ChonkyStation4::Core::Loader
