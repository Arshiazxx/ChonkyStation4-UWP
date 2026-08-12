#pragma once

#include <string>

namespace ChonkyStation4::Core::Loader {

struct ModuleDependency {
    std::string name;
    bool required = true;
    bool resolved = false;
};

} // namespace ChonkyStation4::Core::Loader
