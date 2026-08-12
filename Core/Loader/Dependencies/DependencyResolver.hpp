#pragma once

#include "Core/Loader/Module/ModuleRegistry.hpp"

#include <string>
#include <vector>

namespace ChonkyStation4::Core::Loader {

struct DependencyResolutionReport {
    bool success = false;
    std::vector<std::string> loadOrder;
    std::vector<std::string> missingDependencies;
    std::vector<std::string> circularDependencies;
    std::string log;
    std::string error;
};

class DependencyResolver final {
public:
    explicit DependencyResolver(const ModuleRegistry& registry) noexcept;

    DependencyResolutionReport Resolve(const std::string& moduleName) const;

private:
    const ModuleRegistry* registry_ = nullptr;
};

} // namespace ChonkyStation4::Core::Loader
