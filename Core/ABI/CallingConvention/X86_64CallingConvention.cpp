#include "X86_64CallingConvention.hpp"

namespace ChonkyStation4::Core::ABI {

const std::array<CPU::RegisterId, X86_64CallingConvention::RegisterArgumentCount>&
X86_64CallingConvention::ArgumentRegisters() noexcept {
    static constexpr std::array<CPU::RegisterId, RegisterArgumentCount> registers{
        CPU::RegisterId::Rdi,
        CPU::RegisterId::Rsi,
        CPU::RegisterId::Rdx,
        CPU::RegisterId::Rcx,
        CPU::RegisterId::R8,
        CPU::RegisterId::R9,
    };
    return registers;
}

CPU::RegisterId X86_64CallingConvention::ReturnRegister() noexcept {
    return CPU::RegisterId::Rax;
}

bool X86_64CallingConvention::TryGetArgumentRegister(
    std::size_t index,
    CPU::RegisterId& registerId) noexcept {
    if (index >= RegisterArgumentCount) {
        return false;
    }
    registerId = ArgumentRegisters()[index];
    return true;
}

bool X86_64CallingConvention::TryGetArgument(
    const CPU::CpuState& state,
    std::size_t index,
    std::uint64_t& value) noexcept {
    CPU::RegisterId registerId{};
    if (!TryGetArgumentRegister(index, registerId)) {
        return false;
    }
    value = state.registers[registerId];
    return true;
}

bool X86_64CallingConvention::TrySetArgument(
    CPU::CpuState& state,
    std::size_t index,
    std::uint64_t value) noexcept {
    CPU::RegisterId registerId{};
    if (!TryGetArgumentRegister(index, registerId)) {
        return false;
    }
    state.registers[registerId] = value;
    return true;
}

std::uint64_t X86_64CallingConvention::ReturnValue(
    const CPU::CpuState& state) noexcept {
    return state.registers[ReturnRegister()];
}

void X86_64CallingConvention::SetReturnValue(
    CPU::CpuState& state,
    std::uint64_t value) noexcept {
    state.registers[ReturnRegister()] = value;
}

} // namespace ChonkyStation4::Core::ABI
