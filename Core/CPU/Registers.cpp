#include "Registers.hpp"

namespace ChonkyStation4::Core::CPU {

const char* Registers::Name(RegisterId id) noexcept {
    switch (id) {
    case RegisterId::Rax: return "RAX";
    case RegisterId::Rbx: return "RBX";
    case RegisterId::Rcx: return "RCX";
    case RegisterId::Rdx: return "RDX";
    case RegisterId::Rsi: return "RSI";
    case RegisterId::Rdi: return "RDI";
    case RegisterId::Rbp: return "RBP";
    case RegisterId::Rsp: return "RSP";
    case RegisterId::R8: return "R8";
    case RegisterId::R9: return "R9";
    case RegisterId::R10: return "R10";
    case RegisterId::R11: return "R11";
    case RegisterId::R12: return "R12";
    case RegisterId::R13: return "R13";
    case RegisterId::R14: return "R14";
    case RegisterId::R15: return "R15";
    }
    return "INVALID";
}

} // namespace ChonkyStation4::Core::CPU
