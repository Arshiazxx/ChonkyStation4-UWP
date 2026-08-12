#include "Relocation.hpp"

#include "Core/Memory/GuestMemory.hpp"

#include <cstring>
#include <limits>
#include <sstream>

namespace ChonkyStation4::Core::Loader {

namespace {

bool AddSignedOffset(
    std::uint64_t base,
    std::int64_t offset,
    std::uint64_t& result) noexcept {
    if (offset >= 0) {
        const auto unsignedOffset = static_cast<std::uint64_t>(offset);
        if (unsignedOffset > (std::numeric_limits<std::uint64_t>::max)() - base) {
            return false;
        }
        result = base + unsignedOffset;
        return true;
    }

    const auto magnitude = static_cast<std::uint64_t>(-(offset + 1)) + 1;
    if (magnitude > base) {
        return false;
    }
    result = base - magnitude;
    return true;
}

RelocationResult Unsupported(const RelocationRecord& relocation) {
    RelocationResult result;
    result.status = RelocationStatus::Unsupported;
    result.error = "unsupported ELF relocation: ";
    result.error += RelocationTypeName(relocation.type);
    result.log = result.error;
    return result;
}

} // namespace

const char* RelocationTypeName(RelocationType type) noexcept {
    switch (type) {
    case RelocationType::Absolute64:
        return "R_X86_64_64";
    case RelocationType::GlobDat64:
        return "R_X86_64_GLOB_DAT";
    case RelocationType::JumpSlot64:
        return "R_X86_64_JUMP_SLOT";
    case RelocationType::Relative64:
        return "R_X86_64_RELATIVE";
    case RelocationType::Unknown:
    default:
        return "R_X86_64_UNKNOWN";
    }
}

RelocationResult SafeRelocationResolver::Resolve(
    const RelocationRecord& relocation,
    RelocationContext& context) const {
    if (relocation.type != RelocationType::Relative64) {
        return Unsupported(relocation);
    }

    if (context.moduleSize < sizeof(std::uint64_t) ||
        relocation.offset > context.moduleSize - sizeof(std::uint64_t)) {
        RelocationResult result;
        result.status = RelocationStatus::OutOfBounds;
        result.error = "ELF relocation target is outside the loaded module";
        result.log = result.error;
        return result;
    }
    if (relocation.offset > static_cast<std::uint64_t>((std::numeric_limits<std::int64_t>::max)())) {
        RelocationResult result;
        result.status = RelocationStatus::OutOfBounds;
        result.error = "ELF relocation offset cannot be represented safely";
        result.log = result.error;
        return result;
    }

    std::uint64_t targetAddress = 0;
    if (!AddSignedOffset(context.moduleBase, static_cast<std::int64_t>(relocation.offset), targetAddress)) {
        RelocationResult result;
        result.status = RelocationStatus::OutOfBounds;
        result.error = "ELF relocation target address overflows";
        result.log = result.error;
        return result;
    }

    std::uint64_t value = 0;
    if (!AddSignedOffset(context.moduleBase, relocation.addend, value)) {
        RelocationResult result;
        result.status = RelocationStatus::Invalid;
        result.error = "ELF relative relocation value overflows";
        result.log = result.error;
        return result;
    }

    std::string error;
    if (!context.memory.Write(targetAddress, &value, sizeof(value), &error)) {
        RelocationResult result;
        result.status = RelocationStatus::Invalid;
        result.error = "unable to apply ELF relocation: " + error;
        result.log = result.error;
        return result;
    }

    RelocationResult result;
    result.status = RelocationStatus::Applied;
    result.value = value;
    std::ostringstream log;
    log << "applied " << RelocationTypeName(relocation.type)
        << " at guest address 0x" << std::hex << targetAddress;
    result.log = log.str();
    return result;
}

} // namespace ChonkyStation4::Core::Loader
