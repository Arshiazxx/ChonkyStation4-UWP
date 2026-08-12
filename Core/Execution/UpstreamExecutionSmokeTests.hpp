#pragma once

#include <string>

namespace ChonkyStation4::Core::Execution {

struct UpstreamExecutionSmokeTestReport {
    bool passed = false;
    std::string log;
    std::string failure;
};

UpstreamExecutionSmokeTestReport RunUpstreamExecutionSmokeTests();

} // namespace ChonkyStation4::Core::Execution
