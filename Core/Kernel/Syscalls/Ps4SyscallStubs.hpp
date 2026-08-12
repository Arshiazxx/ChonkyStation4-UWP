#pragma once

#include "Core/Kernel/Syscalls/SyscallTypes.hpp"

namespace ChonkyStation4::Core::Kernel::Ps4SyscallStubs {

SyscallResult ProcessExit(SyscallContext& context);
SyscallResult GetProcessInfo(SyscallContext& context);
SyscallResult CreateThread(SyscallContext& context);
SyscallResult StartThread(SyscallContext& context);
SyscallResult ExitThread(SyscallContext& context);
SyscallResult AllocateMemory(SyscallContext& context);
SyscallResult ReleaseMemory(SyscallContext& context);

} // namespace ChonkyStation4::Core::Kernel::Ps4SyscallStubs
