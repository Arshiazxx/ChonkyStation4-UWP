# Old Xbox/UWP Port — Reference Inventory

**M0 status:** reference-only. No code in this document is authoritative upstream code.

This inventory preserves the useful *ideas and validation findings* from the previous local Xbox/UWP experiment without copying an old emulator snapshot into current upstream.

## Provenance boundary

- Authoritative emulator baseline: `liuk7071/ChonkyStation4@310269290a3c256f5911d4bc7e441489bffffbf6`.
- Public `momo-AUX1/master`: `038aade7dbe4a79bae27c8cfadc4ecb608a6e255`, which is already in upstream history and has no public fork-only `master` commits.
- The prior Xbox/UWP files described below came from a separate local experimental working tree documented in prior project records. That working tree was not supplied to M0, so these items are not copied or treated as source truth.

## Reference components

### Xbox/UWP host

Previously referenced files:

- `platform/xbox_uwp/ChonkyStation4.Xbox.vcxproj`
- `XboxApp.cpp/.hpp`
- `XboxPlatform.cpp/.hpp`
- `Package.appxmanifest`
- Xbox visual assets

Reported state: native C++ executable compiled/linked in the prior Windows environment, but AppX packaging later failed with `APPX1673` requiring `PhoneIdentity`. This configuration is therefore **INVESTIGATE / REIMPLEMENT**, not preserve-as-is.

### D3D12 shell

Previously referenced:

- `XboxD3D12Renderer.cpp/.hpp`

It was described as a D3D12/UWP shell, not a complete PS4 GPU backend. Treat the architecture idea as **INVESTIGATE** and reimplement only after current upstream GPU boundaries are audited.

### Capability probe

Previously referenced standalone application:

- `platform/xbox_uwp/XboxCapabilityProbe/XboxCapabilityProbe.vcxproj`
- `XboxCapabilityProbe.cpp`
- `README.md`

The prior validation design separated the following capabilities rather than assuming them:

- reserve vs reserve+commit;
- fixed-address reservation and exact address equality;
- 2 TiB reserve attempt at `0x0800000000`;
- RWX and RW→RX/RWX protection behavior;
- multiple large reservations;
- address-space probes around `0x0800000000`, `0x8000000000`, `0x001000000000`, and `0x700000000000`;
- a tiny generated x86-64 thunk returning `42` to distinguish executable allocation from actual code execution.

No Xbox hardware result was established in those records. Preserve this **test design**, not any claimed capability result.

### Xbox virtual memory contract

Previously referenced:

- `XboxVirtualMemory.hpp`
- `XboxVirtualMemory.cpp`

Useful contract properties to preserve conceptually:

- relocatable vs exact-address reservation;
- explicit requested/returned addresses;
- reserve/commit/decommit/release/protect/query;
- alignment and allocation-granularity handling;
- explicit Win32/HRESULT failure reporting;
- exact-address operations must never silently accept a relocated address.

Reimplement only after M5 capability evidence and current M6 guest-memory audit.

### ABI/TLS findings

Previous audit records identified two important future gates:

- PS4 guest calls rely on SysV x86-64 ABI behavior while MSVC/UWP uses the Windows x64 ABI;
- guest TLS-related code had assumptions involving host TLS/TEB layout and generated patches.

These findings are **PRESERVE / INVESTIGATE**, but implementation belongs to the separate M7/JIT/CPU-execution gate and is explicitly not started here.

## Rejected migration patterns

- Do not merge the old public fork wholesale.
- Do not copy old emulator core/loader/kernel/renderer snapshots over current upstream.
- Do not import SDL/Vulkan/pthread-win32 into UWP merely to make compilation pass.
- Do not add `PhoneIdentity` simply to suppress an AppX error.
- Do not assume desktop Win32 memory/JIT behavior equals Xbox Dev Mode.
- Do not claim D3D12, executable memory, fixed mapping, or guest execution works on Xbox without runtime evidence.
