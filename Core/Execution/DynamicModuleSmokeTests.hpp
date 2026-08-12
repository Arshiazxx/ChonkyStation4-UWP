#pragma once

#include <string>

namespace ChonkyStation4::Core::Execution {

struct DynamicModuleSmokeTestReport {
    bool passed = false;
    std::string log;
    std::string failure;
};

DynamicModuleSmokeTestReport RunDynamicModuleSmokeTests();

} // namespace ChonkyStation4::Core::Execution
