#pragma once

#include <string>

namespace ChonkyStation4::Core::ABI {

struct AbiSmokeTestReport {
    bool passed = false;
    std::string log;
    std::string failure;
};

AbiSmokeTestReport RunAbiSmokeTests();

} // namespace ChonkyStation4::Core::ABI
