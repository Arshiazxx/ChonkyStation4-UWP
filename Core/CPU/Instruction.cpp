#include "Instruction.hpp"

#include <iomanip>
#include <sstream>

namespace ChonkyStation4::Core::CPU {

namespace {

bool IsRegisterEncodingValid(std::uint8_t value) noexcept {
    return value < RegisterCount;
}

std::uint32_t ReadU32(const std::uint8_t* bytes) {
    return static_cast<std::uint32_t>(bytes[0]) |
        (static_cast<std::uint32_t>(bytes[1]) << 8) |
        (static_cast<std::uint32_t>(bytes[2]) << 16) |
        (static_cast<std::uint32_t>(bytes[3]) << 24);
}

std::uint64_t ReadU64(const std::uint8_t* bytes) {
    return static_cast<std::uint64_t>(bytes[0]) |
        (static_cast<std::uint64_t>(bytes[1]) << 8) |
        (static_cast<std::uint64_t>(bytes[2]) << 16) |
        (static_cast<std::uint64_t>(bytes[3]) << 24) |
        (static_cast<std::uint64_t>(bytes[4]) << 32) |
        (static_cast<std::uint64_t>(bytes[5]) << 40) |
        (static_cast<std::uint64_t>(bytes[6]) << 48) |
        (static_cast<std::uint64_t>(bytes[7]) << 56);
}

std::string Hex(std::uint64_t value) {
    std::ostringstream text;
    text << "0x" << std::uppercase << std::hex << value;
    return text.str();
}

} // namespace

DecodeResult InstructionDecoder::Decode(const std::uint8_t* bytes, std::size_t size) const {
    DecodeResult result;
    if (bytes == nullptr || size == 0) {
        result.error = "instruction bytes are empty";
        return result;
    }

    const auto opcode = bytes[0];
    const auto encodedLength = EncodedLength(opcode);
    if (size < encodedLength) {
        result.error = "instruction is truncated";
        return result;
    }

    result.instruction.encodedSize = encodedLength;
    result.instruction.opcode = static_cast<Opcode>(opcode);
    switch (result.instruction.opcode) {
    case Opcode::Nop:
    case Opcode::Halt:
        result.success = true;
        return result;

    case Opcode::MovImmediate:
    case Opcode::AddImmediate:
    case Opcode::SubImmediate:
        if (!IsRegisterEncodingValid(bytes[1])) {
            result.error = "instruction contains an invalid register";
            return result;
        }
        result.instruction.destination = static_cast<RegisterId>(bytes[1]);
        result.instruction.immediate = ReadU64(bytes + 2);
        result.success = true;
        return result;

    case Opcode::Load:
    case Opcode::Store:
        if (!IsRegisterEncodingValid(bytes[1]) || !IsRegisterEncodingValid(bytes[2])) {
            result.error = "memory instruction contains an invalid register";
            return result;
        }
        result.instruction.destination = static_cast<RegisterId>(bytes[1]);
        result.instruction.source = static_cast<RegisterId>(bytes[2]);
        result.instruction.displacement = static_cast<std::int32_t>(ReadU32(bytes + 3));
        result.success = true;
        return result;
    }

    std::ostringstream error;
    error << "invalid synthetic opcode " << Hex(opcode);
    result.error = error.str();
    return result;
}

std::size_t InstructionDecoder::EncodedLength(std::uint8_t opcode) noexcept {
    switch (static_cast<Opcode>(opcode)) {
    case Opcode::Nop:
    case Opcode::Halt:
        return 1;
    case Opcode::MovImmediate:
    case Opcode::AddImmediate:
    case Opcode::SubImmediate:
        return 10;
    case Opcode::Load:
    case Opcode::Store:
        return 7;
    }
    // An unknown opcode is fetched as one byte so the decoder can return a
    // useful invalid-instruction error without reading past the faulting byte.
    return 1;
}

std::string InstructionDecoder::Format(const Instruction& instruction) {
    std::ostringstream text;
    switch (instruction.opcode) {
    case Opcode::Nop:
        return "NOP";
    case Opcode::MovImmediate:
        text << "MOV " << Registers::Name(instruction.destination) << ", "
             << instruction.immediate;
        return text.str();
    case Opcode::AddImmediate:
        text << "ADD " << Registers::Name(instruction.destination) << ", "
             << instruction.immediate;
        return text.str();
    case Opcode::SubImmediate:
        text << "SUB " << Registers::Name(instruction.destination) << ", "
             << instruction.immediate;
        return text.str();
    case Opcode::Load:
        text << "LOAD " << Registers::Name(instruction.destination) << ", ["
             << Registers::Name(instruction.source);
        if (instruction.displacement >= 0) {
            text << " + " << instruction.displacement;
        } else {
            text << " - " << -static_cast<std::int64_t>(instruction.displacement);
        }
        text << "]";
        return text.str();
    case Opcode::Store:
        text << "STORE " << Registers::Name(instruction.destination) << ", ["
             << Registers::Name(instruction.source);
        if (instruction.displacement >= 0) {
            text << " + " << instruction.displacement;
        } else {
            text << " - " << -static_cast<std::int64_t>(instruction.displacement);
        }
        text << "]";
        return text.str();
    case Opcode::Halt:
        return "HALT";
    }
    return "INVALID";
}

} // namespace ChonkyStation4::Core::CPU
