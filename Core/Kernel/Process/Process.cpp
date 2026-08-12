#include "Process.hpp"

namespace ChonkyStation4::Core::Kernel {

Process::Process(ProcessId id, Memory::GuestMemory& addressSpace) noexcept
    : id_(id), addressSpace_(&addressSpace) {}

ProcessId Process::Id() const noexcept {
    return id_;
}

Memory::GuestMemory& Process::AddressSpace() noexcept {
    return *addressSpace_;
}

const Memory::GuestMemory& Process::AddressSpace() const noexcept {
    return *addressSpace_;
}

ProcessState Process::State() const noexcept {
    return state_;
}

void Process::SetState(ProcessState state) noexcept {
    state_ = state;
}

bool Process::LoadExecutable(const std::string& path, std::string* error) {
    Loader::Elf64Loader loader;
    Loader::ElfLoadReport report;
    if (!loader.LoadIntoMemory(path, *addressSpace_, &report)) {
        state_ = ProcessState::Faulted;
        exceptionMessage_ = report.error;
        if (error != nullptr) {
            *error = report.error;
        }
        return false;
    }

    executable_.loaded = true;
    executable_.path = report.filePath;
    executable_.architecture = report.architecture;
    executable_.entryPoint = report.entryPoint;
    executable_.loadableSegments = report.loadableSegments;
    state_ = ProcessState::Ready;
    exceptionMessage_.clear();
    return true;
}

const LoadedExecutable& Process::Executable() const noexcept {
    return executable_;
}

void Process::AttachThread(ThreadId id) {
    threadIds_.push_back(id);
    if (state_ == ProcessState::Created) {
        state_ = ProcessState::Ready;
    }
}

const std::vector<ThreadId>& Process::ThreadIds() const noexcept {
    return threadIds_;
}

void Process::Terminate(std::int64_t exitCode) noexcept {
    exitCode_ = exitCode;
    state_ = ProcessState::Terminated;
    exceptionMessage_.clear();
}

void Process::Fault(const std::string& message) {
    state_ = ProcessState::Faulted;
    exceptionMessage_ = message;
}

std::int64_t Process::ExitCode() const noexcept {
    return exitCode_;
}

const std::string& Process::ExceptionMessage() const noexcept {
    return exceptionMessage_;
}

} // namespace ChonkyStation4::Core::Kernel
