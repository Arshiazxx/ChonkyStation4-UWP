#pragma once

#include "Core/CPU/CpuState.hpp"
#include "Core/Common/VirtualAddress.hpp"
#include "Core/Kernel/Process/Process.hpp"

#include <cstdint>

namespace ChonkyStation4::Core::Kernel {

enum class ThreadState {
    Created,
    Ready,
    Running,
    Halted,
    Exited,
    Faulted,
};

struct StackInfo {
    GuestVirtualAddress baseAddress = InvalidGuestVirtualAddress;
    std::uint64_t size = 0;
    GuestVirtualAddress topAddress = InvalidGuestVirtualAddress;
};

class Thread final {
public:
    Thread(
        ThreadId id,
        Process& process,
        GuestVirtualAddress entryPoint,
        GuestVirtualAddress stackBase,
        std::uint64_t stackSize) noexcept;

    ThreadId Id() const noexcept;
    Process& OwnerProcess() noexcept;
    const Process& OwnerProcess() const noexcept;

    ThreadState State() const noexcept;
    void SetState(ThreadState state) noexcept;

    CPU::CpuState& Cpu() noexcept;
    const CPU::CpuState& Cpu() const noexcept;
    const StackInfo& Stack() const noexcept;

    void MarkRunning() noexcept;
    void MarkHalted() noexcept;
    void MarkExited(std::int64_t exitCode = 0) noexcept;
    void MarkFaulted(const std::string& message);
    std::int64_t ExitCode() const noexcept;
    const std::string& ExceptionMessage() const noexcept;

private:
    ThreadId id_ = 0;
    Process* process_ = nullptr;
    CPU::CpuState cpu_{};
    StackInfo stack_{};
    ThreadState state_ = ThreadState::Created;
    std::int64_t exitCode_ = 0;
    std::string exceptionMessage_;
};

} // namespace ChonkyStation4::Core::Kernel
