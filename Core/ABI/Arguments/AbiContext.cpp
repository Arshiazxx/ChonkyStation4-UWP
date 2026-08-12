#include "AbiContext.hpp"

#include "Core/ABI/CallingConvention/X86_64CallingConvention.hpp"

namespace ChonkyStation4::Core::ABI {

AbiContext::AbiContext(CPU::CpuState& state) noexcept
    : state_(&state) {}

CPU::CpuState& AbiContext::Cpu() noexcept {
    return *state_;
}

const CPU::CpuState& AbiContext::Cpu() const noexcept {
    return *state_;
}

std::uint64_t AbiContext::SyscallNumber() const noexcept {
    return state_->registers[CPU::RegisterId::Rax];
}

void AbiContext::SetSyscallNumber(std::uint64_t number) noexcept {
    state_->registers[CPU::RegisterId::Rax] = number;
}

bool AbiContext::TryGetArgument(
    std::size_t index,
    std::uint64_t& value) const noexcept {
    return X86_64CallingConvention::TryGetArgument(*state_, index, value);
}

bool AbiContext::TrySetArgument(
    std::size_t index,
    std::uint64_t value) noexcept {
    return X86_64CallingConvention::TrySetArgument(*state_, index, value);
}

std::uint64_t AbiContext::ReturnValue() const noexcept {
    return X86_64CallingConvention::ReturnValue(*state_);
}

void AbiContext::SetReturnValue(std::uint64_t value) noexcept {
    X86_64CallingConvention::SetReturnValue(*state_, value);
}

} // namespace ChonkyStation4::Core::ABI
