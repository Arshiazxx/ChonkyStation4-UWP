#include "SyscallRegistry.hpp"

#include <utility>

namespace ChonkyStation4::Core::Kernel {

bool SyscallRegistry::Register(
    const SyscallMetadata& metadata,
    SyscallHandler handler,
    std::string* error) {
    if (metadata.number == 0) {
        if (error != nullptr) {
            *error = "syscall number cannot be zero";
        }
        return false;
    }
    if (metadata.name.empty()) {
        if (error != nullptr) {
            *error = "syscall name cannot be empty";
        }
        return false;
    }
    if (!handler) {
        if (error != nullptr) {
            *error = "syscall handler is empty";
        }
        return false;
    }
    if (entries_.find(metadata.number) != entries_.end()) {
        if (error != nullptr) {
            *error = "syscall number is already registered";
        }
        return false;
    }
    entries_.emplace(metadata.number, SyscallEntry{metadata, std::move(handler)});
    return true;
}

const SyscallEntry* SyscallRegistry::Find(SyscallNumber number) const noexcept {
    const auto found = entries_.find(number);
    return found == entries_.end() ? nullptr : &found->second;
}

const SyscallMetadata* SyscallRegistry::Metadata(
    SyscallNumber number) const noexcept {
    const auto* entry = Find(number);
    return entry == nullptr ? nullptr : &entry->metadata;
}

std::size_t SyscallRegistry::Size() const noexcept {
    return entries_.size();
}

} // namespace ChonkyStation4::Core::Kernel
