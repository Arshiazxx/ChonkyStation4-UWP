#pragma once

#include "Core/CPU/CpuState.hpp"

#include <cstddef>
#include <cstdint>

namespace ChonkyStation4::Core::ABI {

class AbiContext final {
public:
    explicit AbiContext(CPU::CpuState& state) noexcept;

    CPU::CpuState& Cpu() noexcept;
    const CPU::CpuState& Cpu() const noexcept;

    std::uint64_t SyscallNumber() const noexcept;
    void SetSyscallNumber(std::uint64_t number) noexcept;

    bool TryGetArgument(std::size_t index, std::uint64_t& value) const noexcept;
    bool TrySetArgument(std::size_t index, std::uint64_t value) noexcept;

    std::uint64_t ReturnValue() const noexcept;
    void SetReturnValue(std::uint64_t value) noexcept;

private:
    CPU::CpuState* state_ = nullptr;
};

} // namespace ChonkyStation4::Core::ABI
