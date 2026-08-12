#pragma once

#include <cstdint>

namespace ChonkyStation4::Core::CPU {

// These bit positions match the architecturally useful x86-64 RFLAGS bits,
// while the synthetic executor only updates the arithmetic subset.
enum class Flag : std::uint64_t {
    Carry = 1ull << 0,
    Zero = 1ull << 6,
    Sign = 1ull << 7,
    Overflow = 1ull << 11,
};

class Flags final {
public:
    Flags() = default;

    void Reset() noexcept { value_ = 0; }

    std::uint64_t Raw() const noexcept { return value_; }
    bool IsSet(Flag flag) const noexcept {
        return (value_ & static_cast<std::uint64_t>(flag)) != 0;
    }
    void Set(Flag flag, bool enabled) noexcept {
        const auto bit = static_cast<std::uint64_t>(flag);
        if (enabled) {
            value_ |= bit;
        } else {
            value_ &= ~bit;
        }
    }

    void UpdateAdd(
        std::uint64_t left,
        std::uint64_t right,
        std::uint64_t result) noexcept;
    void UpdateSub(
        std::uint64_t left,
        std::uint64_t right,
        std::uint64_t result) noexcept;

private:
    std::uint64_t value_ = 0;
};

} // namespace ChonkyStation4::Core::CPU
