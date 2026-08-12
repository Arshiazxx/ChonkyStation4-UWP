# ChonkyStation4 Xbox/UWP Project Status

## Source of truth

- Repository: `liuk7071/ChonkyStation4`
- Verified upstream source commit: `310269290a3c256f5911d4bc7e441489bffffbf6`
- Previous public fork `momo-AUX1/ChonkyStation4`: reference only; its current public master contributes no fork-only commits over the verified merge-base.
- Current implementation commit (latest source change before this status document): `14acef1ede2b17b837668d1c13edb6e6cce84636`
- The final documentation/export commit is reported with the delivered ZIP; embedding a commit's own SHA in its tracked contents is self-referential.

## Milestone status

| Milestone | Status | What is validated |
|---|---|---|
| M0 — Correct upstream baseline | **PASS** | Source archive SHA provenance, fork relationship, migration plan, clean imported working tree; baseline build blocker reproduced. |
| M1 — Architecture audit | **PASS** | Current upstream architecture, host dependencies, renderer/memory/native-execution boundaries documented. |
| M2 — Platform boundary extraction | **PASS** | Renderer factory seam and SDL removal from generic renderer interface pass source/static validation. Existing Vulkan remains default. |
| M3 — Xbox UWP host | **PARTIAL** | UWP project/manifest/lifecycle/diagnostics/gamepad/storage sources and static project validation pass. Windows build/package/runtime are not available here. |
| M4 — D3D12 foundation | **PARTIAL** | Device/adapter/queue/list/swap-chain/descriptors/resources/shaders/fence and three render demos pass static validation. No Windows/Xbox render execution was performed. |
| M5 — Xbox capability probe | **PARTIAL** | Probe source and NOT_RUN machine-readable baseline pass validation. Actual Xbox results require hardware execution. Generated guest/JIT code is deliberately not executed. |
| M6 — Guest memory foundation | **PARTIAL** | Upstream guest layout mirrored, UWP-aware data-memory diagnostic implemented, and constants/source coverage pass validation. Exact 2 TiB reservation and mappings require Xbox execution. Executable guest memory remains M7. |

`PARTIAL` means implementation exists but one or more mandatory target-platform validations could not be performed in this environment. It is not a claim of Xbox runtime success.

## Current architecture

```text
Current ChonkyStation4 core
        |
        +-- GCN / PM4 processing
        |       -> Renderer interface
        |           -> RendererFactory
        |               -> VulkanRenderer (current emulator backend)
        |               -> future real D3D12 PS4 backend
        |
        +-- current guest loader / kernel / memory model
        |       -> future Xbox memory/runtime integration
        |
        `-- existing emulator subsystems

Xbox/UWP side
        |
        +-- ChonkyStation4.Xbox host
        |       -> lifecycle / diagnostics / LocalFolder / Gamepad
        |       -> SwapChainPanel
        |           -> M4 D3D12 host diagnostic backend
        |
        +-- XboxCapabilityProbe
        |       -> memory / VM / threading / graphics / UWP probes
        |       -> xbox-capabilities.json
        |
        `-- XboxGuestMemory diagnostic
                -> exact upstream layout checks
                -> xbox-guest-memory.json
```

## Xbox readiness summary

### Implemented

- clean renderer creation seam without changing GCN PM4 decoding;
- SDL ownership removed from the generic renderer interface;
- isolated UWP/Xbox host project and manifest;
- lifecycle, logging, LocalFolder and controller foundations;
- standalone host-side D3D12 foundation with clear/triangle/textured-quad diagnostics;
- dedicated capability probe with explicit `SUPPORTED`, `UNSUPPORTED`, `UNKNOWN`, and `REQUIRES_HARDWARE_TEST` classifications;
- guest data-memory primitive layer and exact-layout diagnostic;
- checked-in NOT_RUN JSON baselines that do not fabricate Xbox results;
- repository-side static validators for M3–M6.

### Not established

- successful MSBuild compilation of either UWP solution;
- AppX/MSIX packaging in this environment;
- launch on Xbox Dev Mode hardware;
- D3D12 render output on Xbox;
- actual Xbox memory limits, descriptor/resource limits, feature level, shader model, audio/input/network behavior;
- whether the upstream 2 TiB exact reservation succeeds on Xbox;
- executable guest code, guest TLS/ABI, native guest execution.

## Known blockers and validation gaps

### Windows/UWP toolchain unavailable

The current execution host is Linux. `msbuild` is not installed, so both Xbox solutions return exit code 127 when a real MSBuild command is attempted. The source/project validators pass, but this cannot replace a Windows compilation.

### Upstream desktop dependency archive incomplete

The authoritative source was supplied as a GitHub source archive. Its `.gitmodules` file is present, but submodule contents are absent. CMake therefore stops before compilation on missing Zydis, SDL, xxHash, miniz, cpp-httplib, glslang and cppco sources; Vulkan development files are also unavailable here.

### M7 native-execution gate

Current upstream executable mappings and code patching still assume executable memory behavior that cannot simply be transferred to UWP. M6 intentionally implements data memory only. No guest code is invoked and no TLS/native-execution workaround has been introduced.

## Recommended M7 strategy

M7 should begin only after collecting **actual Xbox** output from both `XboxCapabilityProbe` and the M6 guest-memory diagnostic.

Recommended order:

1. Build/package the probe on a supported Windows UWP toolchain.
2. Run it on Xbox Dev Mode and archive the generated `xbox-capabilities.json` and `xbox-guest-memory.json` without hand-editing results.
3. Decide whether the exact 2 TiB reservation and required protection transitions are feasible.
4. Audit current upstream `ELFLoader`, `CodePatcher`, TLS handling and pthread-win32/native ABI assumptions against those measurements.
5. Design a W→RX code generation/cache model if permitted; do not introduce RWX as a convenience workaround.
6. Validate near-address code patch allocation and instruction-cache handling.
7. Validate guest TLS/GS-base assumptions independently before attempting a full guest entry point.
8. Only then wire native guest execution into the Xbox host.

M7 is **NOT STARTED** by this tree.
