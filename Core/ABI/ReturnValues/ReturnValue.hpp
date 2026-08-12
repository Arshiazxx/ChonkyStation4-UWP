#pragma once

#include "Core/CPU/CpuState.hpp"

#include <cstdint>

namespace ChonkyStation4::Core::ABI {

struct ReturnValue {
    std::uint64_t raw = 0;

    std::int64_t Signed() const noexcept {
        return static_cast<std::int64_t>(raw);
    }
};

ReturnValue ReadReturnValue(const CPU::CpuState& state) noexcept;
void WriteReturnValue(CPU::CpuState& state, ReturnValue value) noexcept;

} // namespace ChonkyStation4::Core::ABI
