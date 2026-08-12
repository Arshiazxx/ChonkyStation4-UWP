#pragma once

#include "Core/CPU/Registers.hpp"

#include <cstddef>
#include <cstdint>
#include <string>

namespace ChonkyStation4::Core::CPU {

// M8 deliberately uses a small private encoding. It is an execution harness,
// not an assertion that these bytes are the final PS4/x86-64 decoder format.
enum class Opcode : std::uint8_t {
    Nop = 0x00,
    MovImmediate = 0x01,
    AddImmediate = 0x02,
    SubImmediate = 0x03,
    Load = 0x04,
    Store = 0x05,
    Halt = 0xff,
};

struct Instruction {
    static constexpr std::size_t MaxEncodedSize = 10;

    Opcode opcode = Opcode::Nop;
    RegisterId destination = RegisterId::Rax;
    RegisterId source = RegisterId::Rax;
    std::uint64_t immediate = 0;
    std::int32_t displacement = 0;
    std::size_t encodedSize = 0;
};

struct DecodeResult {
    bool success = false;
    Instruction instruction{};
    std::string error;
};

class InstructionDecoder final {
public:
    DecodeResult Decode(const std::uint8_t* bytes, std::size_t size) const;

    static std::size_t EncodedLength(std::uint8_t opcode) noexcept;
    static std::string Format(const Instruction& instruction);
};

} // namespace ChonkyStation4::Core::CPU
