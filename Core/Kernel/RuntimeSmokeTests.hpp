#pragma once

#include <string>

namespace ChonkyStation4::Core::Kernel {

struct RuntimeSmokeTestReport {
    bool passed = false;
    std::string log;
    std::string failure;
};

RuntimeSmokeTestReport RunRuntimeSmokeTests();

} // namespace ChonkyStation4::Core::Kernel
