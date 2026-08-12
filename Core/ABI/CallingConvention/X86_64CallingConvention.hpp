#pragma once

#include "Core/CPU/CpuState.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace ChonkyStation4::Core::ABI {

class X86_64CallingConvention final {
public:
    static constexpr std::size_t RegisterArgumentCount = 6;

    static const std::array<CPU::RegisterId, RegisterArgumentCount>& ArgumentRegisters() noexcept;
    static CPU::RegisterId ReturnRegister() noexcept;

    static bool TryGetArgumentRegister(
        std::size_t index,
        CPU::RegisterId& registerId) noexcept;

    static bool TryGetArgument(
        const CPU::CpuState& state,
        std::size_t index,
        std::uint64_t& value) noexcept;

    static bool TrySetArgument(
        CPU::CpuState& state,
        std::size_t index,
        std::uint64_t value) noexcept;

    static std::uint64_t ReturnValue(const CPU::CpuState& state) noexcept;
    static void SetReturnValue(CPU::CpuState& state, std::uint64_t value) noexcept;
};

} // namespace ChonkyStation4::Core::ABI
