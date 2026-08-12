#pragma once

#include "Core/Loader/Module/LoadedModule.hpp"

#include <string>
#include <vector>

namespace ChonkyStation4::Core::Loader {

class ModuleRegistry final {
public:
    bool Register(const LoadedModule& module, std::string* error = nullptr);
    bool RegisterMain(const LoadedModule& module, std::string* error = nullptr);
    bool Remove(const std::string& name, std::string* error = nullptr);

    LoadedModule* Find(const std::string& name) noexcept;
    const LoadedModule* Find(const std::string& name) const noexcept;
    LoadedModule* FindByPath(const std::string& path) noexcept;
    const LoadedModule* FindByPath(const std::string& path) const noexcept;
    LoadedModule* MainModule() noexcept;
    const LoadedModule* MainModule() const noexcept;

    const std::vector<LoadedModule>& Modules() const noexcept;
    std::size_t Size() const noexcept;

private:
    std::vector<LoadedModule> modules_;
    std::string mainModuleName_;
};

} // namespace ChonkyStation4::Core::Loader
