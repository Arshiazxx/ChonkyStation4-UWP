#include "ReturnValue.hpp"

#include "Core/ABI/CallingConvention/X86_64CallingConvention.hpp"

namespace ChonkyStation4::Core::ABI {

ReturnValue ReadReturnValue(const CPU::CpuState& state) noexcept {
    return {X86_64CallingConvention::ReturnValue(state)};
}

void WriteReturnValue(CPU::CpuState& state, ReturnValue value) noexcept {
    X86_64CallingConvention::SetReturnValue(state, value.raw);
}

} // namespace ChonkyStation4::Core::ABI
