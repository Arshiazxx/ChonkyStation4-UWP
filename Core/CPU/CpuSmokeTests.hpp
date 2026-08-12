#pragma once

#include <string>

namespace ChonkyStation4::Core::CPU {

struct CpuSmokeTestReport {
    bool passed = false;
    std::string log;
    std::string failure;
};

CpuSmokeTestReport RunCpuSmokeTests();

} // namespace ChonkyStation4::Core::CPU
