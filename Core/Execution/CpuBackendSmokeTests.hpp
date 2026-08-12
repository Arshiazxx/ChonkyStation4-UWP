#pragma once

#include <string>

namespace ChonkyStation4::Core::Execution {

struct CpuBackendSmokeTestReport {
    bool passed = false;
    std::string log;
    std::string failure;
};

CpuBackendSmokeTestReport RunCpuBackendSmokeTests();

} // namespace ChonkyStation4::Core::Execution
