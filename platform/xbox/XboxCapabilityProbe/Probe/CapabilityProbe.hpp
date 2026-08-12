#pragma once

#include <string>
#include <vector>

namespace XboxCapabilityProbe {
namespace Probe {

struct ProbeEntry {
    std::string category;
    std::string name;
    std::string status;
    std::string observed;
    std::string detail;
};

struct ProbeReport {
    std::string sourceCommit;
    std::string deviceFamily;
    std::string runState;
    std::vector<ProbeEntry> entries;

    std::string ToJson() const;
    std::wstring ToSummary() const;
};

class CapabilityProbe final {
public:
    static ProbeReport Run();
    static Platform::String^ WriteJson(const ProbeReport& report);
};

} // namespace Probe
} // namespace XboxCapabilityProbe
