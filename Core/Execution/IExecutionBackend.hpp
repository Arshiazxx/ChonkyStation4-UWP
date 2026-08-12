#pragma once

#include <string>

namespace ChonkyStation4::Core::Execution {

class ExecutionContext;

struct ExecutionBoundaryResult {
    bool available = false;
    bool accepted = false;
    bool executed = false;
    std::string backend;
    std::string message;
    std::string error;
};

class IExecutionBackend {
public:
    virtual ~IExecutionBackend() = default;

    virtual const char* Name() const noexcept = 0;
    virtual bool IsAvailable() const noexcept = 0;

    // This is the common backend boundary. Native/upstream-compatible
    // backends must not enter guest code until their mapping, ABI, TLS,
    // exception, and security contracts are implemented. The synthetic M8
    // backend may execute only its private test encoding.
    virtual ExecutionBoundaryResult Start(ExecutionContext& context) const = 0;
};

// The shorter name describes the intended architecture while retaining the
// IExecutionBackend spelling used by the M11/M12 code.
using ExecutionBackend = IExecutionBackend;

} // namespace ChonkyStation4::Core::Execution
