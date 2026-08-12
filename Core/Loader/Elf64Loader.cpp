#include "Elf64Loader.hpp"

#include "Core/Memory/GuestMemory.hpp"

#include <algorithm>
#include <array>
#include <fstream>
#include <iomanip>
#include <limits>
#include <new>
#include <sstream>
#include <vector>

namespace ChonkyStation4::Core::Loader {

namespace {

constexpr std::uint8_t ElfMagic0 = 0x7f;
constexpr std::uint8_t ElfMagic1 = 'E';
constexpr std::uint8_t ElfMagic2 = 'L';
constexpr std::uint8_t ElfMagic3 = 'F';
constexpr std::uint8_t ElfClass64 = 2;
constexpr std::uint8_t ElfDataLittleEndian = 1;
constexpr std::uint8_t ElfCurrentVersion = 1;
constexpr std::uint16_t MachineX86_64 = 62;
constexpr std::uint32_t ProgramTypeLoad = 1;
constexpr std::uint32_t ProgramFlagExecute = 1u;
constexpr std::uint32_t ProgramFlagWrite = 2u;
constexpr std::uint32_t ProgramFlagRead = 4u;
constexpr std::uint16_t ElfHeaderSize64 = 64;
constexpr std::uint16_t ElfProgramHeaderSize64 = 56;

bool AddOverflows(std::uint64_t base, std::uint64_t size) {
    return size > (std::numeric_limits<std::uint64_t>::max)() - base;
}

std::uint16_t ReadU16(const std::uint8_t* bytes) {
    return static_cast<std::uint16_t>(bytes[0]) |
        (static_cast<std::uint16_t>(bytes[1]) << 8);
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

bool ReadAt(
    std::ifstream& file,
    std::uint64_t fileSize,
    std::uint64_t offset,
    void* destination,
    std::size_t size,
    std::string* error) {
    if (size == 0) {
        return true;
    }
    if (offset > fileSize || static_cast<std::uint64_t>(size) > fileSize - offset) {
        if (error != nullptr) {
            *error = "requested ELF data is outside the file";
        }
        return false;
    }

    file.clear();
    file.seekg(static_cast<std::streamoff>(offset), std::ios::beg);
    if (!file.good()) {
        if (error != nullptr) {
            *error = "unable to seek to requested ELF data";
        }
        return false;
    }
    file.read(static_cast<char*>(destination), static_cast<std::streamsize>(size));
    if (file.gcount() != static_cast<std::streamsize>(size)) {
        if (error != nullptr) {
            *error = "unable to read requested ELF data";
        }
        return false;
    }
    return true;
}

void ParseHeader(const std::array<std::uint8_t, ElfHeaderSize64>& bytes, Elf64Header& header) {
    std::copy(bytes.begin(), bytes.begin() + 16, header.ident);
    header.type = ReadU16(bytes.data() + 16);
    header.machine = ReadU16(bytes.data() + 18);
    header.version = ReadU32(bytes.data() + 20);
    header.entryPoint = ReadU64(bytes.data() + 24);
    header.programHeaderOffset = ReadU64(bytes.data() + 32);
    header.sectionHeaderOffset = ReadU64(bytes.data() + 40);
    header.flags = ReadU32(bytes.data() + 48);
    header.headerSize = ReadU16(bytes.data() + 52);
    header.programHeaderSize = ReadU16(bytes.data() + 54);
    header.programHeaderCount = ReadU16(bytes.data() + 56);
    header.sectionHeaderSize = ReadU16(bytes.data() + 58);
    header.sectionHeaderCount = ReadU16(bytes.data() + 60);
    header.sectionNameIndex = ReadU16(bytes.data() + 62);
}

Elf64ProgramHeader ParseProgramHeader(
    const std::array<std::uint8_t, ElfProgramHeaderSize64>& bytes) {
    Elf64ProgramHeader header;
    header.type = ReadU32(bytes.data());
    header.flags = ReadU32(bytes.data() + 4);
    header.fileOffset = ReadU64(bytes.data() + 8);
    header.virtualAddress = ReadU64(bytes.data() + 16);
    header.physicalAddress = ReadU64(bytes.data() + 24);
    header.fileSize = ReadU64(bytes.data() + 32);
    header.memorySize = ReadU64(bytes.data() + 40);
    header.alignment = ReadU64(bytes.data() + 48);
    return header;
}

std::string FileName(const std::string& path) {
    const auto separator = path.find_last_of("\\/");
    return separator == std::string::npos ? path : path.substr(separator + 1);
}

std::string ArchitectureName(std::uint16_t machine) {
    if (machine == MachineX86_64) {
        return "x86-64";
    }
    std::ostringstream text;
    text << "Unknown (machine " << machine << ")";
    return text.str();
}

std::uint32_t SegmentPermissions(std::uint32_t flags) {
    std::uint32_t permissions = Memory::ToPermissions(Memory::MemoryPermission::None);
    if ((flags & ProgramFlagRead) != 0) {
        permissions |= Memory::ToPermissions(Memory::MemoryPermission::Read);
    }
    if ((flags & ProgramFlagWrite) != 0) {
        permissions |= Memory::ToPermissions(Memory::MemoryPermission::Write);
    }
    if ((flags & ProgramFlagExecute) != 0) {
        permissions |= Memory::ToPermissions(Memory::MemoryPermission::Execute);
    }
    return permissions;
}

void SetError(ElfLoadReport& report, const std::string& error) {
    report.success = false;
    report.error = error;
}

} // namespace

ElfLoadReport Elf64Loader::LoadFile(const std::string& path) const {
    ElfLoadReport report;
    report.filePath = path;

    std::ifstream file(path, std::ios::binary | std::ios::in);
    if (!file.is_open()) {
        SetError(report, "unable to open ELF file");
        return report;
    }

    file.seekg(0, std::ios::end);
    const auto end = file.tellg();
    if (end < 0) {
        SetError(report, "unable to determine ELF file size");
        return report;
    }
    const auto fileSize = static_cast<std::uint64_t>(end);

    if (fileSize < ElfHeaderSize64) {
        SetError(report, "file is smaller than an ELF64 header");
        return report;
    }

    std::array<std::uint8_t, ElfHeaderSize64> headerBytes{};
    if (!ReadAt(file, fileSize, 0, headerBytes.data(), headerBytes.size(), &report.error)) {
        SetError(report, report.error);
        return report;
    }

    if (headerBytes[0] != ElfMagic0 || headerBytes[1] != ElfMagic1 ||
        headerBytes[2] != ElfMagic2 || headerBytes[3] != ElfMagic3) {
        SetError(report, "invalid ELF magic");
        return report;
    }
    if (headerBytes[4] != ElfClass64) {
        SetError(report, "ELF file is not 64-bit");
        return report;
    }
    if (headerBytes[5] != ElfDataLittleEndian) {
        SetError(report, "only little-endian ELF files are supported");
        return report;
    }
    if (headerBytes[6] != ElfCurrentVersion) {
        SetError(report, "unsupported ELF identification version");
        return report;
    }

    ParseHeader(headerBytes, report.header);
    report.format = "ELF64";
    report.entryPoint = report.header.entryPoint;
    report.architectureName = ArchitectureName(report.header.machine);
    report.architecture = report.header.machine == MachineX86_64
        ? ElfArchitecture::X86_64
        : ElfArchitecture::Unknown;

    if (report.header.version != ElfCurrentVersion) {
        SetError(report, "unsupported ELF header version");
        return report;
    }
    if (report.header.headerSize != ElfHeaderSize64) {
        SetError(report, "unexpected ELF64 header size");
        return report;
    }
    if (report.architecture != ElfArchitecture::X86_64) {
        SetError(report, "unsupported ELF architecture: " + report.architectureName);
        return report;
    }
    if (report.header.programHeaderCount != 0 &&
        report.header.programHeaderSize < ElfProgramHeaderSize64) {
        SetError(report, "ELF program header entries are smaller than ELF64 requires");
        return report;
    }
    if (report.header.programHeaderCount != 0 &&
        (report.header.programHeaderOffset > fileSize ||
         static_cast<std::uint64_t>(report.header.programHeaderSize) *
             report.header.programHeaderCount > fileSize - report.header.programHeaderOffset)) {
        SetError(report, "ELF program header table is outside the file");
        return report;
    }

    report.programHeaders.reserve(report.header.programHeaderCount);
    for (std::uint16_t index = 0; index < report.header.programHeaderCount; ++index) {
        const auto offset = report.header.programHeaderOffset +
            static_cast<std::uint64_t>(index) * report.header.programHeaderSize;
        std::array<std::uint8_t, ElfProgramHeaderSize64> programHeaderBytes{};
        if (!ReadAt(file, fileSize, offset, programHeaderBytes.data(), programHeaderBytes.size(),
                    &report.error)) {
            SetError(report, report.error);
            return report;
        }

        const auto programHeader = ParseProgramHeader(programHeaderBytes);
        report.programHeaders.push_back(programHeader);
        if (programHeader.type != ProgramTypeLoad) {
            continue;
        }
        if (programHeader.memorySize < programHeader.fileSize) {
            SetError(report, "ELF loadable segment memory size is smaller than file size");
            return report;
        }
        if (AddOverflows(programHeader.fileOffset, programHeader.fileSize) ||
            programHeader.fileOffset + programHeader.fileSize > fileSize) {
            SetError(report, "ELF loadable segment reaches outside the file");
            return report;
        }
        if (AddOverflows(programHeader.virtualAddress, programHeader.memorySize)) {
            SetError(report, "ELF loadable segment virtual address range overflows");
            return report;
        }

        report.loadableSegments.push_back({
            programHeader.fileOffset,
            programHeader.virtualAddress,
            programHeader.physicalAddress,
            programHeader.fileSize,
            programHeader.memorySize,
            programHeader.alignment,
            programHeader.flags,
        });
    }

    report.success = true;
    return report;
}

bool Elf64Loader::LoadIntoMemory(
    const std::string& path,
    Memory::GuestMemory& memory,
    ElfLoadReport* report) const {
    auto parsed = LoadFile(path);
    if (!parsed.success) {
        if (report != nullptr) {
            *report = parsed;
        }
        return false;
    }

    std::ifstream file(path, std::ios::binary | std::ios::in);
    if (!file.is_open()) {
        SetError(parsed, "unable to reopen ELF file for segment loading");
        if (report != nullptr) {
            *report = parsed;
        }
        return false;
    }
    file.seekg(0, std::ios::end);
    const auto end = file.tellg();
    if (end < 0) {
        SetError(parsed, "unable to determine ELF file size for segment loading");
        if (report != nullptr) {
            *report = parsed;
        }
        return false;
    }
    const auto fileSize = static_cast<std::uint64_t>(end);

    std::vector<GuestVirtualAddress> mappedAddresses;
    for (const auto& segment : parsed.loadableSegments) {
        if (segment.memorySize == 0) {
            continue;
        }
        if (segment.fileSize > (std::numeric_limits<std::size_t>::max)()) {
            SetError(parsed, "ELF segment file data is too large for this host");
            break;
        }

        std::vector<std::uint8_t> initialData;
        try {
            initialData.resize(static_cast<std::size_t>(segment.fileSize));
        } catch (const std::bad_alloc&) {
            SetError(parsed, "unable to allocate ELF segment staging storage");
            break;
        }

        if (!ReadAt(file, fileSize, segment.fileOffset, initialData.data(), initialData.size(),
                    &parsed.error)) {
            SetError(parsed, parsed.error);
            break;
        }

        Memory::MemoryRegion region;
        region.baseAddress = segment.virtualAddress;
        region.size = segment.memorySize;
        region.permissions = SegmentPermissions(segment.flags);
        region.name = "ELF PT_LOAD";

        std::string error;
        if (!memory.Map(region, initialData.data(), initialData.size(), &error)) {
            SetError(parsed, "unable to map ELF loadable segment: " + error);
            break;
        }
        mappedAddresses.push_back(region.baseAddress);
    }

    if (!parsed.success) {
        for (const auto address : mappedAddresses) {
            memory.Unmap(address);
        }
    }
    if (report != nullptr) {
        *report = parsed;
    }
    return parsed.success;
}

std::string ElfLoadReport::ToText() const {
    std::ostringstream text;
    text << "ChonkyStation4 Loader\n\n"
         << "File:\n" << FileName(filePath) << "\n\n"
         << "Format:\n" << format << "\n\n"
         << "Architecture:\n" << architectureName << "\n\n"
         << "Entry Point:\n0x" << std::uppercase << std::hex << entryPoint
         << std::dec << "\n\n"
         << "Segments:\n" << loadableSegments.size() << "\n";

    for (std::size_t index = 0; index < loadableSegments.size(); ++index) {
        const auto& segment = loadableSegments[index];
        text << "  [" << index << "] VA 0x" << std::uppercase << std::hex
             << segment.virtualAddress << "  File 0x" << segment.fileSize
             << "  Memory 0x" << segment.memorySize << "  Flags 0x"
             << segment.flags << std::dec << "\n";
    }

    if (!error.empty()) {
        text << "\nError:\n" << error << "\n";
    }
    text << "\nLoad Status:\n" << (success ? "Success" : "Failure") << "\n";
    return text.str();
}

} // namespace ChonkyStation4::Core::Loader
