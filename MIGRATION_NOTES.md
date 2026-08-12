# ChonkyStation4 Xbox/UWP Migration Notes

**Date:** 2026-08-10  
**Milestone:** M0 — Correct Upstream Baseline  
**Status:** **PASS** (baseline compile is blocked by source-archive submodule omission / local Vulkan SDK absence; blocker validated and documented below)

## M0 outcome

M0 establishes the current `liuk7071/ChonkyStation4` source as the only authoritative emulator baseline. No Xbox/UWP emulator code was merged into upstream during M0, and M1 has not started.

The supplied archive was verified as a GitHub source archive for upstream commit:

`310269290a3c256f5911d4bc7e441489bffffbf6` — **Bump version**

The old public fork `momo-AUX1/ChonkyStation4` is a historical upstream snapshot, not a divergent Xbox branch. Its current public `master` is:

`038aade7dbe4a79bae27c8cfadc4ecb608a6e255` — **Merge pull request #1 from kalaposfos13/linux**

That commit exists in the authoritative upstream history and is the exact merge-base for the two current public `master` branches. Current upstream is 74 commits ahead; the public fork contributes zero fork-only `master` commits.

## 1. Authoritative source verification

- Authoritative repository: `https://github.com/liuk7071/ChonkyStation4`
- Supplied archive: `ChonkyStation4-master.zip`
- Expected upstream commit: `310269290a3c256f5911d4bc7e441489bffffbf6`
- ZIP comment: `310269290a3c256f5911d4bc7e441489bffffbf6`
- Archive SHA-256: `f5dfac2d9bf144b4541c1e7bff8e399e3d6a8017fe171699ed7e2a594a14e9a7`
- ZIP integrity test: **PASS** (`unzip -t` reported no errors)
- `.git` directory in archive: **absent** (normal for GitHub source ZIPs)
- `.gitmodules`: **present**
- Current archive marker: `CMakeLists.txt` contains `VERSION_STRING "alpha-2"`, matching the `Bump version` commit.
- Current archive marker: `ChonkyStation4/Loaders/App/AppLoader.cpp` contains the System Menu name assignment introduced by the same head commit.

The archive is therefore accepted as the current upstream source snapshot at the requested SHA. The SHA is not inferred from the filename; it is embedded in the archive comment and cross-checked against the authoritative GitHub commit.

## 2. Git metadata and clean working tree

The supplied GitHub source ZIP does not contain `.git`, so original commit objects/history cannot be reconstructed from the archive alone.

A local Git working tree was created at:

`/mnt/data/chonkystation4-xbox-port`

Configured remotes:

- `origin` → `https://github.com/liuk7071/ChonkyStation4.git`
- `momo` → `https://github.com/momo-AUX1/ChonkyStation4.git`

Before M0 documentation was added, a recursive content comparison against the pristine extracted archive reported no differences (excluding the newly created `.git` directory).

Local synthetic import commit:

`703ad5a9b06393ee6c17ec8446072f6411c315ca`

**Important:** this local SHA is not claimed to be the upstream Git commit. It is only a local commit representing the supplied archive contents so that subsequent port work has a clean, auditable working tree.

## 3. Exact public fork relationship

### Public fork head / merge-base

`momo-AUX1/master` = `038aade7dbe4a79bae27c8cfadc4ecb608a6e255`

The same commit is present in `liuk7071/ChonkyStation4` history. GitHub's cross-fork comparison reports 74 commits in `liuk7071:master` after `momo-AUX1:master`, with 162 files changed. Therefore:

- exact current public merge-base: `038aade7dbe4a79bae27c8cfadc4ecb608a6e255`
- public fork-only `master` commits: **0**
- upstream-only commits after the fork head: **74**
- public fork `master` is not an Xbox/UWP implementation branch; despite its WinRT-oriented description, its visible `master` contains no fork-only WinRT/UWP/Xbox work to cherry-pick.

### 74 upstream commits missing from current `momo-AUX1/master`

The range is:

`038aade7dbe4a79bae27c8cfadc4ecb608a6e255..310269290a3c256f5911d4bc7e441489bffffbf6`

1. `3102692` — Bump version
2. `33e9bf4` — GCN: Fix detiler
3. `70bf4bd` — Vulkan: Add GPU selector command
4. `5e87efc` — Initialize /dev/urandom
5. `9a31a79` — Add CLI options, fix cmake error
6. `7c4d194` — Fix regressions
7. `cebcf4a` — VSH stuff part 2
8. `b8cb652` — OS: Stuff for VSH
9. `03c10cf` — Fix cmake error
10. `4ce3c7f` — Vulkan: Integrate NVIDIA Aftermath SDK to generate GPU crash dumps. OS: PlayGo fixes, filesystem fixes, others.
11. `788089f` — Vulkan: Implement depth clip and others. OS: Implement _readv and fix pthread_once
12. `4659a83` — Vulkan: Fix texture garbage collector
13. `591b930` — Vulkan: Fix compute shader memory leak and others
14. `9665ead` — Experimental red zone patch
15. `683d258` — GCN: Async compute queue fixes, other stuff
16. `a932f6a` — GCN: Fix some bugs
17. `a327a9f` — GCN: Implement instructions, add GDS
18. `a010ad8` — Vulkan: more optimizations
19. `ec1b542` — GCN: Add barriers after DS instructions, handle one more V_CMP_CLASS case
20. `648fc2d` — GCN: Detect loops, implement more instructions
21. `5e897f5` — Vulkan: optimizations
22. `450d404` — GCN: control flow fixes (add block discovery pass)
23. `04e784a` — GCN: if/else control flow
24. `f6abae5` — OS: sceKernelReserveVirtualRange fixes, other stuff. GCN: Clean up vector compares
25. `a2bd611` — OS: TCP sockets, other stuff for LLE HTTP + Epolls. SceNpWebApi and SceNpMatching2 stuff
26. `1d484d5` — OS: Implement UDP sockets (sceNet). Add CLI commands to be used by the launcher
27. `b890058` — PSN: Initial ChonkyNet integration
28. `a0cef4f` — GCN: 3D textures, other stuff. OS: Many things
29. `b14ebaa` — OS: Fix sceSslGetCaCerts compile error. GCN: Handle EXP vm bit
30. `7faff9f` — OS: Stub some things
31. `8a86ee9` — Replace zlib-ng with miniz
32. `776ec12` — fix sceZlib lock bug
33. `f121d74` — OS: SceZlib
34. `c195829` — GCN: DrawIndexOffset2 and many instructions. OS: sceNpMatching2 fixes, TLS fixes, other
35. `b3ef3d2` — GCN: Compute pipelines (DispatchDirect) and implement some instructions
36. `fa0069b` — Push changes (I forgot what I added but it's a lot)
37. `c74933a` — Vulkan: Fix render target and texture stuff. OS: More function stubs
38. `db8b8af` — OS: pthread fixes, stub some functions. GCN: Implement some instructions, handle shaders with no fetch shader, fix a few things
39. `34be6db` — Vulkan: Performance improvements, implement IMAGE_GATHER4. OS: Many TLS fixes, implement some functions. And other stuff
40. `b826ca8` — OS: More sceNpMatching stuff
41. `f22edf6` — OS: Begin working on Np (sceNpManager, sceNpMatching)
42. `7e49959` — OS: Fix dirent thing
43. `89d72c9` — Vulkan: Indexed indirect draws, implement some instructions. OS: pthread fixes
44. `af270a1` — Update README.md
45. `eb8dcb5` — OS: Implement/stub a few things
46. `bd0d491` — OS: sceKernelFtruncate, stub a few things. GCN: Implement a few instructions
47. `8403932` — GCN: 32bit index type
48. `628261a` — Vulkan: Pipeline cache fixes, handle other libSceVideoOut buffer formats
49. `90d78d3` — Vulkan: Fix memory leaks, improve pipeline cache. OS: sceVideoOut vblank stuff, implement and stub many functions
50. `8935690` — OS: Fix uninitialized TID bug. GCN: Begin implementing stencil, fix a few things, implement some instructions
51. `4dd3baa` — Vulkan: Implement Rect and Quad lists
52. `dbfc6af` — OS: sceKernelGetdents, sceRandom, stub some things
53. `590bd39` — Vulkan: Fix -W...+W clip space
54. `9b7119a` — GCN: Shader decompiler improvements, viewport depth. OS: sceVideoOutAddVblankEvent stub
55. `15640be` — Vulkan: Improve shader cache
56. `f2011e2` — GCN/Vulkan: Render targets
57. `1b14807` — GCN: Implement TBUFFER_LOAD_FORMAT_* and many other instructions. Vulkan: Fix pipeline cache bug. OS: Implement semaphore timeouts and other pthread stuff
58. `d60642f` — Loader: SELF loader. OS: sceKernelLoadStartModule
59. `b1736c9` — OS: scePad light bar and vibration
60. `91e728a` — OS: pthread get/set specific. GCN: Constant engine
61. `5d97778` — OS: sceVideoOut and sceRtc stuff, stub some network stuff, sceGnmSubmitCommandBuffers, pthread fixes. GCN: Implement many instructions
62. `9147a3d` — Vulkan: Buffer/Texture cache rewrite
63. `6708351` — oops
64. `66cd6bd` — Vulkan: Texture tracking system
65. `bdede37` — Vulkan: Blending, better pipeline hashing. GCN: Implement the remaining SetRegister packets
66. `0a819aa` — GCN: Implement many instructions, fix some bugs. Vulkan: Use VulkanMemoryAllocator. OS: SaveData stuff
67. `8a5009b` — GCN: Shader decompiler fixes, implement some texture formats. OS: sceAudioOut fixes
68. `cc2fde7` — OS: Fix sceKernelGetTscFrequency. GCN: Some fixes
69. `7fdb774` — OS: pthread fixes, implement some functions. Vulkan: small optimizations
70. `ad91fc1` — OS: sceSaveDataMount
71. `effe2da` — OS: User system, basic savedata, FS writes
72. `3a35cf3` — OS: Stub network stuff, fix filesystem bug. GCN: IndirectBuffer packet
73. `85e9690` — GCN: Fix command processor bug. OS: Partial LLE library system
74. `bf2b3eb` — GCN: Begin implementing compute queues

### Port-impact summary of that upstream delta

The 74-commit delta is materially relevant to an Xbox port. It includes, among other changes:

- major GCN/Vulkan renderer evolution, including render targets, caches, compute, control flow, texture handling, GPU selection, and detiling;
- guest/host memory behavior changes such as `sceKernelReserveVirtualRange` fixes and an experimental red-zone patch;
- multiple TLS and pthread fixes, which directly invalidate assumptions made by an older execution/TLS port;
- SELF/LLE loader work and module-loading changes;
- filesystem, savedata, directory, PlayGo, random-device and I/O changes;
- audio and controller/pad changes;
- timing/TSC fixes;
- UDP/TCP/epoll/PSN networking work;
- dependency/build changes, notably VMA and replacement of zlib-ng with miniz.

Therefore no emulator subsystem implementation from the old local Xbox experiment should replace the current upstream version wholesale.

## 4. Public `momo-AUX1` changes relevant to Xbox/UWP categories

Because the current public fork head is itself an upstream commit and contributes zero fork-only commits, the public fork has **no fork-specific changes** to classify in these requested categories:

- WinRT
- UWP
- Xbox
- D3D12
- memory
- filesystem
- input
- audio
- threading
- platform abstraction
- application hosting

For the public fork, the migration classification is therefore **DROP as a code source**: it is an outdated upstream snapshot, not an authoritative implementation source.

This does **not** mean the previous local Xbox/UWP work was useless. It means that work was not committed to the public `momo-AUX1/master` lineage and must be handled separately as reference material.

## 5. Previous local Xbox/UWP experiment — reference-only classification

Previous project records describe an experimental local Xbox port with a UWP host, D3D12 shell, platform files, a capability probe and an Xbox virtual-memory abstraction. The old source tree itself was not supplied in this M0 input, so no source file from that experiment has been copied into the clean upstream tree.

| Previous item / concept | M0 classification | Reason / action |
|---|---|---|
| `XboxPlatform.*` platform seam | **REIMPLEMENT** | Preserve the isolation concept, but implement it against current upstream interfaces during M2 rather than copying an unknown old snapshot. |
| `XboxApp.*`, UWP lifecycle host | **REIMPLEMENT** | Useful host concept, but must be rebuilt against current upstream and current UWP project model in M3. |
| Old `ChonkyStation4.Xbox.vcxproj` / manifest / assets | **INVESTIGATE** | Previous records say native C++ linked but AppX packaging hit `APPX1673` / `PhoneIdentity`; do not import this configuration blindly. Re-evaluate in M3. |
| `XboxD3D12Renderer.*` shell | **INVESTIGATE** | Useful D3D12 bootstrap concept, but it was only a host shell and not the current GPU abstraction. Re-audit and reimplement cleanly in M4. |
| `XboxCapabilityProbe` design | **PRESERVE** | The capability questions/tests are valuable. Reimplement/port the probe in M5 after current architecture is established. Prior runtime results remain unknown. |
| `XboxVirtualMemory` explicit reserve/commit/protect/query contract | **PRESERVE** | The explicit exact-address/error semantics are sound design material. Revalidate against current guest-memory assumptions and Xbox results before integration in M5/M6. |
| Direct fixed-address / executable-memory assumptions | **INVESTIGATE** | Must be measured on Xbox hardware; do not assume desktop Win32 behavior. |
| Previous ABI/TLS findings | **PRESERVE** as documentation, **INVESTIGATE** later | Current upstream still has a native x86-64/SysV-sensitive execution model. Do not implement M7/JIT/ABI work in this task. |
| Copying old Vulkan/SDL/pthread-win32 emulator paths into UWP | **DROP** | Would downgrade current upstream and violate platform isolation. |
| Adding `PhoneIdentity` merely to silence AppX packaging | **DROP** | Not a valid Xbox/UWP migration strategy. |
| Duplicate/experimental Xbox project layouts such as `platform/xbox_uwp` vs `platform/xbox/_uwp` | **INVESTIGATE** then **DROP obsolete duplicates** | The actual previous tree is not present here, so deletion cannot be justified yet. Do not recreate duplicates. |
| Old emulator core/loader/kernel copies from the previous port | **DROP** | Current upstream is 74 commits ahead and authoritative. Reuse only isolated platform ideas, never old core snapshots. |

A separate inventory is kept in `docs/migration/OLD_XBOX_PORT_REFERENCE.md`. It contains no migrated emulator code.

## 6. Current upstream Xbox/UWP state

A direct scan of the supplied current upstream source found no Xbox/UWP/WinRT/D3D12 project files or source paths and no `.vcxproj`, `.sln`, or AppX manifest in the archive. Current upstream is therefore still the desktop emulator baseline; Xbox support must be added as an isolated platform implementation rather than treated as an existing upstream backend.

M1 will determine the precise platform boundaries. M0 intentionally does not perform that architecture redesign.

## 7. Baseline dependency state

The archive contains `.gitmodules`, but GitHub source ZIPs do not embed submodule repository contents. All 13 declared submodule directories are empty in the supplied archive.

Authoritative submodule pins for upstream commit `3102692...` were verified from GitHub:

| Dependency | Upstream gitlink commit |
|---|---|
| `ELFIO` | `7d30a22fc5aac06adfe7887ae57f3701b6b5f913` |
| `SDL` | `c4dbe242c9e34bc107ddcc2647828189c0ea5ea6` |
| `TinySHA1` | `2795aa8de91b1797defdfbff61ed93b22b5ced81` |
| `VulkanMemoryAllocator` | `e722e57c891a8fbe3cc73ca56c19dd76be242759` |
| `asio` | `bd500f0a018db9a845ebaaed5c0318343ae9f497` |
| `cpp-httplib` | `32abac3de5a1a5c57e0b06b4a8c261263c9e6499` |
| `cppco` | `ed8047411bab25c7272d76b39f341130596d6826` |
| `glslang` | `7099c123729e02f81d70559e79ee4360096fdfe5` |
| `miniz` | `2ea4e81e1593c48112ef7fdf1da5562704acfdd2` |
| `pthread-win32` | `b89b3cddee96ab7a9562b9f4b61174029f702a09` |
| `xbyak` | `9d8ff37306f39c6a71cf998078cbe880ce5dc224` |
| `xxHash` | `36cd8bfe01811f2f0b2d3d3c55a785366ba78560` |
| `zydis` | `ae12a099a1323d1535881b50b5afe72cc0894afa` |

The shell in this execution environment cannot resolve GitHub, so these repositories cannot be populated here with `git submodule update --init --recursive` or an equivalent download.

## 8. Untouched upstream baseline build

No emulator source or CMake file was modified before this attempt.

### Configure/build command

```bash
cmake -S /mnt/data/chonkystation4-m0/ChonkyStation4-master \
      -B /mnt/data/chonkystation4-m0/build-pristine \
      -G Ninja \
      -DCMAKE_BUILD_TYPE=Release
```

### Result

**BLOCKED before compilation.** CMake identifies GCC 14.2 successfully, then fails because the GitHub source archive contains empty submodule directories:

- `Dependencies/zydis` — missing `CMakeLists.txt`
- `Dependencies/SDL` — missing `CMakeLists.txt`
- `Dependencies/xxHash/cmake_unofficial` — absent
- `Dependencies/miniz` — missing `CMakeLists.txt`
- `Dependencies/cpp-httplib` — missing `CMakeLists.txt`
- `Dependencies/glslang` — missing `CMakeLists.txt`
- `Dependencies/cppco` — missing `CMakeLists.txt`

The current container also lacks Vulkan development headers/libraries, so `find_package(Vulkan REQUIRED)` fails.

This is not an Xbox-port regression and no workaround was introduced into upstream CMake. A fully populated clone (including submodules) plus a Vulkan SDK/development package is required to compile the untouched desktop baseline.

Log: `/mnt/data/chonkystation4-m0/M0_CONFIGURE.log`

### Build command

Not run after configure failure, because no valid Ninja build graph was generated. This is recorded as **BLOCKED**, not PASS.

## 9. Upstream tests

Search of the top-level emulator tree (excluding empty dependency submodule directories) found no `enable_testing()` or `add_test()` declarations and no project test directory/CTest configuration.

Command executed:

```bash
ctest --test-dir /mnt/data/chonkystation4-m0/build-pristine --output-on-failure
```

Result:

`No tests were found!!!`

Status: **NOT TESTED / NO UPSTREAM TESTS MATERIALIZED**. This is distinct from claiming tests passed.

## 10. Files changed by M0

No current upstream emulator source, renderer, loader, kernel, OS, dependency declaration, or CMake implementation was changed.

M0 additions only:

- `MIGRATION_NOTES.md`
- `docs/migration/OLD_XBOX_PORT_REFERENCE.md`

The local `.git` metadata is workspace state, not upstream source.

## 11. Known issues / blockers carried forward

1. The supplied GitHub archive is verified at the correct root SHA but omits submodule contents by design.
2. Shell networking in this environment cannot resolve GitHub, so submodules cannot be populated locally.
3. Vulkan development files are not installed in this container.
4. There is no Windows MSBuild/UWP SDK in this environment for later UWP project validation.
5. No Xbox Dev Mode console is attached; no Xbox runtime capability is claimed.
6. The previous local Xbox source tree itself was not supplied; only prior validation/configuration records are available. Public `momo-AUX1/master` contains no fork-only Xbox code.

## 12. M0 validation ledger

| Validation item | Result |
|---|---|
| Supplied archive integrity | **PASS** |
| Upstream source SHA verified | **PASS** |
| Archive matched to authoritative current upstream | **PASS** |
| Git metadata inspected | **PASS** — `.git` absent; `.gitmodules` present |
| Exact public fork head identified | **PASS** |
| Exact public merge-base established | **PASS** — `038aade7...` |
| Public fork-only master commits identified | **PASS** — none |
| 74 upstream-only commits identified | **PASS** |
| Xbox/UWP-relevant old work classified | **PASS** — reference-only inventory; no blind merge |
| Clean upstream-based local Git working tree created | **PASS** |
| Untouched upstream configure/build attempted | **PASS** (procedure executed) |
| Untouched baseline compilation | **BLOCKED** — missing submodule contents + Vulkan dev package |
| Available upstream tests run | **PASS** (CTest executed) |
| Upstream test result | **NOT TESTED / NO TESTS FOUND** |
| Current upstream emulator code modified | **NO** |
| M1 started | **NO** |
| **M0 overall** | **PASS** — build blocker is genuine, reproduced, and documented as allowed by M0 acceptance criteria |

## 13. M0 migration decision

M0 is **PASS**.

The clean migration baseline is current `liuk7071` upstream at `310269290a3c256f5911d4bc7e441489bffffbf6`. The public `momo-AUX1/master` is not a source of fork-only Xbox code. Previous local Xbox work remains reference-only and is classified for later selective reimplementation.

Per the current instruction, **M1 has not been started in this task**.
