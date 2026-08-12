#pragma once

#include "Core/Common/VirtualAddress.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace ChonkyStation4::Core::Memory {
class GuestMemory;
}

namespace ChonkyStation4::Core::Loader {

enum class ElfArchitecture {
    Unknown,
    X86_64,
};

struct Elf64Header {
    std::uint8_t ident[16]{};
    std::uint16_t type = 0;
    std::uint16_t machine = 0;
    std::uint32_t version = 0;
    std::uint64_t entryPoint = 0;
    std::uint64_t programHeaderOffset = 0;
    std::uint64_t sectionHeaderOffset = 0;
    std::uint32_t flags = 0;
    std::uint16_t headerSize = 0;
    std::uint16_t programHeaderSize = 0;
    std::uint16_t programHeaderCount = 0;
    std::uint16_t sectionHeaderSize = 0;
    std::uint16_t sectionHeaderCount = 0;
    std::uint16_t sectionNameIndex = 0;
};

struct Elf64ProgramHeader {
    std::uint32_t type = 0;
    std::uint32_t flags = 0;
    std::uint64_t fileOffset = 0;
    std::uint64_t virtualAddress = 0;
    std::uint64_t physicalAddress = 0;
    std::uint64_t fileSize = 0;
    std::uint64_t memorySize = 0;
    std::uint64_t alignment = 0;
};

struct ElfLoadSegment {
    std::uint64_t fileOffset = 0;
    GuestVirtualAddress virtualAddress = 0;
    std::uint64_t physicalAddress = 0;
    std::uint64_t fileSize = 0;
    std::uint64_t memorySize = 0;
    std::uint64_t alignment = 0;
    std::uint32_t flags = 0;
};

struct ElfLoadReport {
    bool success = false;
    std::string filePath;
    std::string format = "Unknown";
    ElfArchitecture architecture = ElfArchitecture::Unknown;
    std::string architectureName = "Unknown";
    std::uint64_t entryPoint = 0;
    Elf64Header header{};
    std::vector<Elf64ProgramHeader> programHeaders;
    std::vector<ElfLoadSegment> loadableSegments;
    std::string error;

    std::string ToText() const;
};

class Elf64Loader final {
public:
    ElfLoadReport LoadFile(const std::string& path) const;

    // Parses an ELF image already held by the caller. This keeps platform-neutral
    // loader tests independent of host file-system policy while preserving the
    // existing LoadFile API for real images.
    ElfLoadReport LoadBytes(
        const std::string& imageName,
        const std::vector<std::uint8_t>& bytes) const;

    // Parses the file, then maps each PT_LOAD segment into the supplied
    // platform-neutral guest memory object. File-backed bytes are copied and
    // the remaining memory-size bytes are zero initialized.
    bool LoadIntoMemory(
        const std::string& path,
        Memory::GuestMemory& memory,
        ElfLoadReport* report = nullptr) const;

    bool LoadBytesIntoMemory(
        const std::string& imageName,
        const std::vector<std::uint8_t>& bytes,
        Memory::GuestMemory& memory,
        ElfLoadReport* report = nullptr) const;
};

} // namespace ChonkyStation4::Core::Loader
