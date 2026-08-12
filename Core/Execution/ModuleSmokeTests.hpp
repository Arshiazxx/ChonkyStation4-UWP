#pragma once

#include <string>

namespace ChonkyStation4::Core::Execution {

struct ModuleSmokeTestReport {
    bool passed = false;
    std::string log;
    std::string failure;
};

ModuleSmokeTestReport RunModuleSmokeTests();

} // namespace ChonkyStation4::Core::Execution
