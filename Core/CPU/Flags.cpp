#include "Flags.hpp"

namespace ChonkyStation4::Core::CPU {

namespace {

bool SignBit(std::uint64_t value) noexcept {
    return (value >> 63) != 0;
}

} // namespace

void Flags::UpdateAdd(
    std::uint64_t left,
    std::uint64_t right,
    std::uint64_t result) noexcept {
    Set(Flag::Carry, result < left);
    Set(Flag::Zero, result == 0);
    Set(Flag::Sign, SignBit(result));
    Set(Flag::Overflow, ((~(left ^ right) & (left ^ result)) >> 63) != 0);
}

void Flags::UpdateSub(
    std::uint64_t left,
    std::uint64_t right,
    std::uint64_t result) noexcept {
    Set(Flag::Carry, left < right);
    Set(Flag::Zero, result == 0);
    Set(Flag::Sign, SignBit(result));
    Set(Flag::Overflow, (((left ^ right) & (left ^ result)) >> 63) != 0);
}

} // namespace ChonkyStation4::Core::CPU
