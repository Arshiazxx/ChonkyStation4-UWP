#include "Ps4SyscallStubs.hpp"

#include "Core/ABI/Arguments/AbiContext.hpp"
#include "Core/Kernel/Scheduler/Scheduler.hpp"
#include "Core/Kernel/Thread/Thread.hpp"

#include <cstddef>
#include <limits>

namespace ChonkyStation4::Core::Kernel::Ps4SyscallStubs {

namespace {

constexpr std::uint64_t PageSize = 0x1000;
constexpr std::uint64_t MaximumAllocation = 0x1000000;

SyscallResult Failure(const char* name, const std::string& message) {
    SyscallResult result;
    result.handlerName = name;
    result.error = message;
    return result;
}

SyscallResult Success(const char* name, std::uint64_t value = 0) {
    SyscallResult result;
    result.success = true;
    result.value = value;
    result.handlerName = name;
    return result;
}

bool GetArgument(
    SyscallContext& context,
    std::size_t index,
    std::uint64_t& value,
    SyscallResult& failure) {
    if (context.abi == nullptr) {
        failure.error = "PS4 ABI context is required for this syscall";
        return false;
    }
    if (!context.abi->TryGetArgument(index, value)) {
        failure.error = "syscall argument index is outside the ABI register set";
        return false;
    }
    return true;
}

bool Overlaps(
    const Memory::GuestMemory& memory,
    GuestVirtualAddress base,
    std::uint64_t size) {
    if (size > (std::numeric_limits<GuestVirtualAddress>::max)() - base) {
        return true;
    }
    const auto end = base + size;
    for (const auto& region : memory.Regions()) {
        const auto regionEnd = region.baseAddress + region.size;
        if (base < regionEnd && region.baseAddress < end) {
            return true;
        }
    }
    return false;
}

bool RoundAllocation(std::uint64_t requested, std::uint64_t& rounded) {
    if (requested == 0 || requested > MaximumAllocation ||
        requested > (std::numeric_limits<std::uint64_t>::max)() - (PageSize - 1)) {
        return false;
    }
    rounded = (requested + PageSize - 1) & ~(PageSize - 1);
    return rounded != 0;
}

} // namespace

SyscallResult ProcessExit(SyscallContext& context) {
    std::uint64_t exitCode = context.cpu.registers[CPU::RegisterId::Rax];
    SyscallResult failure = Failure("sceKernelExitProcess", "");
    if (context.abi != nullptr && !GetArgument(context, 0, exitCode, failure)) {
        return failure;
    }

    context.thread.MarkExited(static_cast<std::int64_t>(exitCode));
    context.process.Terminate(static_cast<std::int64_t>(exitCode));
    return Success("sceKernelExitProcess", exitCode);
}

SyscallResult GetProcessInfo(SyscallContext& context) {
    return Success("sceKernelGetProcessInfo", context.process.Id());
}

SyscallResult CreateThread(SyscallContext& context) {
    SyscallResult result = Failure("sceKernelCreateThread", "");
    if (context.scheduler == nullptr) {
        result.error = "scheduler context is required to create a thread";
        return result;
    }

    std::uint64_t entryPoint = 0;
    std::uint64_t stackBase = 0;
    std::uint64_t stackSize = 0;
    if (!GetArgument(context, 0, entryPoint, result) ||
        !GetArgument(context, 1, stackBase, result) ||
        !GetArgument(context, 2, stackSize, result)) {
        return result;
    }
    if (entryPoint == 0 || stackBase == 0 || stackSize == 0) {
        result.error = "thread entry point and stack arguments must be non-zero";
        return result;
    }

    std::string error;
    auto* thread = context.scheduler->CreateThread(
        context.process,
        entryPoint,
        stackBase,
        stackSize,
        &error);
    if (thread == nullptr) {
        result.error = error.empty() ? "thread creation failed" : error;
        return result;
    }
    return Success("sceKernelCreateThread", thread->Id());
}

SyscallResult StartThread(SyscallContext& context) {
    SyscallResult result = Failure("sceKernelStartThread", "");
    if (context.scheduler == nullptr) {
        result.error = "scheduler context is required to start a thread";
        return result;
    }

    std::uint64_t threadId = 0;
    if (!GetArgument(context, 0, threadId, result)) {
        return result;
    }
    if (threadId > (std::numeric_limits<ThreadId>::max)()) {
        result.error = "thread identifier is out of range";
        return result;
    }
    auto* target = context.scheduler->FindThread(static_cast<ThreadId>(threadId));
    if (target == nullptr || &target->OwnerProcess() != &context.process) {
        result.error = "thread was not found in the process";
        return result;
    }
    if (target->State() != ThreadState::Created &&
        target->State() != ThreadState::Ready) {
        result.error = "thread is not startable in its current state";
        return result;
    }

    // M10 deliberately models the transition only. Scheduler execution remains
    // sequential and will pick up this ready thread on its next run.
    target->SetState(ThreadState::Ready);
    return Success("sceKernelStartThread", threadId);
}

SyscallResult ExitThread(SyscallContext& context) {
    std::uint64_t exitCode = 0;
    SyscallResult result = Failure("sceKernelExitThread", "");
    if (context.abi != nullptr && !GetArgument(context, 0, exitCode, result)) {
        return result;
    }
    context.thread.MarkExited(static_cast<std::int64_t>(exitCode));
    return Success("sceKernelExitThread", 0);
}

SyscallResult AllocateMemory(SyscallContext& context) {
    SyscallResult result = Failure("sceKernelAllocateMemory", "");
    std::uint64_t requestedSize = 0;
    if (!GetArgument(context, 0, requestedSize, result)) {
        return result;
    }

    std::uint64_t allocationSize = 0;
    if (!RoundAllocation(requestedSize, allocationSize)) {
        result.error = "memory allocation size is invalid or too large";
        return result;
    }

    constexpr Memory::MemoryPermissions permissions =
        Memory::ToPermissions(Memory::MemoryPermission::Read) |
        Memory::ToPermissions(Memory::MemoryPermission::Write);
    for (std::uint64_t base = 0x10000000; base < 0x70000000; base += 0x1000000) {
        if (Overlaps(context.process.AddressSpace(), base, allocationSize)) {
            continue;
        }
        const Memory::MemoryRegion region{
            base,
            allocationSize,
            permissions,
            "sceKernelAllocateMemory",
        };
        std::string error;
        if (context.process.AddressSpace().Map(region, nullptr, 0, &error)) {
            return Success("sceKernelAllocateMemory", base);
        }
        result.error = error;
    }

    if (result.error.empty()) {
        result.error = "unable to find a free guest memory range";
    }
    return result;
}

SyscallResult ReleaseMemory(SyscallContext& context) {
    SyscallResult result = Failure("sceKernelReleaseMemory", "");
    std::uint64_t baseAddress = 0;
    if (!GetArgument(context, 0, baseAddress, result)) {
        return result;
    }
    std::string error;
    if (!context.process.AddressSpace().Unmap(baseAddress, &error)) {
        result.error = error.empty() ? "memory region was not mapped" : error;
        return result;
    }
    return Success("sceKernelReleaseMemory", 0);
}

} // namespace ChonkyStation4::Core::Kernel::Ps4SyscallStubs
