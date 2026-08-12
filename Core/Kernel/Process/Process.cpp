#include "Process.hpp"

#include <limits>
#include <utility>

namespace ChonkyStation4::Core::Kernel {

Process::Process(ProcessId id, Memory::GuestMemory& addressSpace) noexcept
    : id_(id), addressSpace_(&addressSpace), moduleManager_(addressSpace) {}

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
    Loader::LoadedModule mainModule(
        "Main Executable", Loader::ModuleKind::MainExecutable);
    if (!Loader::LoadedModule::FromElfReport(path, report, mainModule, error) ||
        !moduleManager_.RegisterMainModule(mainModule, error)) {
        state_ = ProcessState::Faulted;
        exceptionMessage_ = error != nullptr ? *error : "unable to register main executable module";
        return false;
    }
    state_ = ProcessState::Ready;
    exceptionMessage_.clear();
    return true;
}

const LoadedExecutable& Process::Executable() const noexcept {
    return executable_;
}

bool Process::RegisterModule(const Loader::LoadedModule& module, std::string* error) {
    if (!moduleManager_.RegisterModule(module, error)) {
        return false;
    }
    if (state_ == ProcessState::Created) {
        state_ = ProcessState::Ready;
    }
    return true;
}

bool Process::RegisterMainModule(const Loader::LoadedModule& module, std::string* error) {
    if (!moduleManager_.RegisterMainModule(module, error)) {
        return false;
    }
    if (state_ == ProcessState::Created) {
        state_ = ProcessState::Ready;
    }
    return true;
}

const std::vector<Loader::LoadedModule>& Process::Modules() const noexcept {
    return moduleManager_.Registry().Modules();
}

const Loader::LoadedModule* Process::MainModule() const noexcept {
    return moduleManager_.MainModule();
}

Loader::ModuleManager& Process::ModuleManager() noexcept {
    return moduleManager_;
}

const Loader::ModuleManager& Process::ModuleManager() const noexcept {
    return moduleManager_;
}

bool Process::LoadModule(
    const std::string& name,
    const std::string& path,
    Loader::ModuleKind kind,
    std::string* error) {
    if (!moduleManager_.LoadModuleFromFile(name, path, kind, error)) {
        return false;
    }
    if (state_ == ProcessState::Created) {
        state_ = ProcessState::Ready;
    }
    return true;
}

bool Process::LoadModuleBytes(
    const std::string& name,
    const std::string& sourcePath,
    const std::vector<std::uint8_t>& bytes,
    Loader::ModuleKind kind,
    std::string* error) {
    if (!moduleManager_.LoadModuleFromBytes(name, sourcePath, bytes, kind, error)) {
        return false;
    }
    if (state_ == ProcessState::Created) {
        state_ = ProcessState::Ready;
    }
    return true;
}

bool Process::UnloadModule(const std::string& name, std::string* error) {
    return moduleManager_.UnloadModule(name, error);
}

const Loader::LoadedModule* Process::FindModule(const std::string& name) const noexcept {
    return moduleManager_.FindModule(name);
}

Loader::SymbolResolutionResult Process::ResolveSymbol(const std::string& name) const {
    return moduleManager_.ResolveSymbol(name);
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
