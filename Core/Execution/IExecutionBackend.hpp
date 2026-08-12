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

    // Starts no guest or host code by contract. A future backend may use this
    // boundary to hand the context to native execution, a JIT, or the M8
    // synthetic executor.
    virtual ExecutionBoundaryResult Start(ExecutionContext& context) const = 0;
};

} // namespace ChonkyStation4::Core::Execution
