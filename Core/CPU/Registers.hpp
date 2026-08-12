#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace ChonkyStation4::Core::CPU {

enum class RegisterId : std::uint8_t {
    Rax = 0,
    Rbx,
    Rcx,
    Rdx,
    Rsi,
    Rdi,
    Rbp,
    Rsp,
    R8,
    R9,
    R10,
    R11,
    R12,
    R13,
    R14,
    R15,
};

constexpr std::size_t RegisterCount = 16;

class Registers final {
public:
    Registers() = default;

    std::uint64_t& operator[](RegisterId id) noexcept {
        return values_[static_cast<std::size_t>(id)];
    }

    const std::uint64_t& operator[](RegisterId id) const noexcept {
        return values_[static_cast<std::size_t>(id)];
    }

    void Reset() noexcept {
        values_.fill(0);
    }

    static const char* Name(RegisterId id) noexcept;

private:
    std::array<std::uint64_t, RegisterCount> values_{};
};

} // namespace ChonkyStation4::Core::CPU
