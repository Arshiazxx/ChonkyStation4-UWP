#pragma once

#include <cstdint>

namespace ChonkyStation4::Core {

using GuestVirtualAddress = std::uint64_t;

constexpr GuestVirtualAddress InvalidGuestVirtualAddress = 0;

} // namespace ChonkyStation4::Core
