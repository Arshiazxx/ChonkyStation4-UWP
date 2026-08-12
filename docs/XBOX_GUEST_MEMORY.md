# Xbox Guest Memory Foundation

## Milestone

**M6 — Guest Memory Initialization**

Authoritative emulator baseline: `liuk7071/ChonkyStation4` at `310269290a3c256f5911d4bc7e441489bffffbf6`.

Milestone status: **PARTIAL**.

M6 implements a reusable UWP-aware **data-memory** reservation/commit/protection/decommit foundation and a diagnostic that attempts the exact current-upstream address-space layout. It does not modify the emulator's loader, kernel memory syscalls, native guest execution, TLS, or JIT/code patcher.

No Xbox hardware run was available, so the exact 2 TiB reservation and all target results remain unmeasured.

## Current upstream memory model

### Early process reservation

`ChonkyStation4/ChonkyStation4.cpp` attempts, before emulator initialization:

```text
base = 0x0000000080000000
size = 2048 GiB (2 TiB)
state = MEM_RESERVE
protection = PAGE_NOACCESS
```

The resulting reserved range is conceptually:

```text
0x0000000080000000
        ↓
        |<------------- 2048 GiB ------------->|
        ↓                                       ↓
0x0000000080000000                 0x0000020080000000
```

Failure currently emits a warning rather than terminating immediately, but later memory allocation assumes suitable reserved address space exists.

### Kernel allocation search area

The current Win32 kernel helper scans reserved regions and commits memory inside them. Most guest allocations search:

```text
start = 0x0000000080000000
length = 2000 GiB
end = 0x000001F480000000
```

This is intentionally smaller than the early 2048 GiB reservation.

`sceKernelReserveVirtualRange` also contains a separate system-mapping search base:

```text
SYSTEM_MAPPING_AREA = 0x0000001000000000  (64 GiB)
```

when the requested address is below the normal guest allocation base.

### Page and alignment assumptions

The guest-facing `kernel_getpagesize()` reports **16 KiB**.

Current host allocation helpers:

- read the host page size with `GetSystemInfo`;
- clamp requested alignment to at least the host page size;
- round committed size to the host page size;
- commonly request 16 KiB alignment for flexible/anonymous mappings;
- rely on `VirtualQuery` to identify reserved regions before committing them.

The real allocation granularity is therefore also a host capability and is reported by the M5/M6 diagnostics rather than hard-coded as an Xbox fact.

## Current mapping behavior

### Anonymous/direct/flexible memory

Current upstream uses `VirtualAlloc(..., MEM_COMMIT, PAGE_READWRITE)` inside the large pre-reserved range for:

- direct memory mappings;
- flexible memory mappings;
- anonymous `mmap`;
- ELF module storage.

`SCE_KERNEL_MAP_FIXED` is significant. `sceKernelMapDirectMemory` verifies that a fixed request is returned at the requested virtual address and treats a mismatch as failure.

`sceKernelReserveVirtualRange` also tracks logical reservations separately in `reserved_areas`; in one fixed-map path it records the address without making a new host reservation because the process-wide 2 TiB host reservation already covers it.

### Decommit/unmap

`sceKernelMunmap` currently uses `VirtualFree(addr, len, MEM_DECOMMIT)`. This returns pages to the reserved state while keeping the outer address-space reservation intact.

### File-backed mappings

`kernel_mmap` currently obtains a Win32 handle from the CRT `FILE*`, calls `CreateFileMappingW`, and maps with `MapViewOfFile(FILE_MAP_READ)`.

For UWP, M6 does **not** wire the existing filesystem layer to raw handles. The diagnostic only validates the UWP mapping mechanism with a small pagefile-backed mapping using:

- `CreateFileMappingFromApp`;
- `MapViewOfFileFromApp`;
- `UnmapViewOfFile`.

A real guest file-backed `mmap` adapter remains future integration work because it must be coordinated with the Xbox filesystem/storage layer and current kernel FD implementation.

## Protection handling

Current upstream depends on page-protection changes in several places:

1. ELF load segments are currently changed to `PAGE_EXECUTE_READWRITE` as a temporary implementation shortcut.
2. `CodePatcher` allocates `PAGE_EXECUTE_READWRITE` patch regions within ±2 GiB of original native guest code.
3. Vulkan buffer dirty tracking changes guest pages between read-only and read/write and uses access violations to detect CPU writes.

M6 implements only **non-executable data protections**:

- `PAGE_NOACCESS` for reservation;
- `PAGE_READWRITE` for committed data;
- `PAGE_READONLY` / `PAGE_READWRITE` transitions through `VirtualProtectFromApp`.

There is deliberately no executable protection enum or generated-code path in `XboxGuestMemory`.

## UWP-specific memory boundary

M6 introduces:

```text
platform/xbox/Memory/XboxGuestMemory.hpp
platform/xbox/Memory/XboxGuestMemory.cpp
```

The boundary exposes:

```text
ReserveUpstreamAddressSpace()
Commit(offset, size)
Protect(offset, size, data protection)
Decommit(offset, size)
Release()
```

This is not yet connected to the emulator kernel. Its purpose is to prove the primitive operations and exact layout before replacing current direct Win32 calls in a later integration milestone.

The implementation uses UWP-aware APIs:

```text
VirtualAllocFromApp
VirtualProtectFromApp
VirtualQuery
VirtualFree
CreateFileMappingFromApp
MapViewOfFileFromApp
```

The exact reservation request remains **0x80000000 + 2048 GiB**. It is not silently relocated if the requested base fails.

## Diagnostic

`XboxCapabilityProbe` now has a second action:

**Run guest memory diagnostic**

It attempts, in order:

1. query host page size and allocation granularity;
2. reserve exactly 2048 GiB at `0x0000000080000000`;
3. verify the reservation with `VirtualQuery`;
4. commit at least one 16 KiB guest page at the reservation base;
5. write/read sentinel bytes;
6. transition committed data to read-only;
7. restore read/write;
8. decommit the committed range;
9. create/map/write/read a 64 KiB pagefile-backed shared mapping;
10. release the 2 TiB reservation.

It writes `xbox-guest-memory.json` to the app LocalFolder. The repository-root file of the same name is a **NOT_RUN baseline** and contains no fabricated target result.

The diagnostic never starts PS4 loading or guest execution.

## Exact-address requirements

The following current-upstream assumptions are considered hard requirements until proven otherwise:

| Requirement | Current upstream behavior | Xbox status |
|---|---|---|
| 64-bit process | Native pointers hold the guest layout | `REQUIRES_HARDWARE_TEST` / project targets x64 |
| Reserve base `0x80000000` | Explicit early reservation | `REQUIRES_HARDWARE_TEST` |
| Reserve 2048 GiB | Explicit early reservation | `REQUIRES_HARDWARE_TEST` |
| Search/commit inside 2000 GiB window | Kernel allocator behavior | `REQUIRES_HARDWARE_TEST` |
| Fixed direct mappings | Fixed requests must match | `REQUIRES_HARDWARE_TEST` |
| 16 KiB guest page contract | Kernel reports 16 KiB | IMPLEMENTED AS CONTRACT |
| Read/write commit | Required broadly | `REQUIRES_HARDWARE_TEST` |
| Read-only/read-write transitions | GPU dirty tracking and data protection | `REQUIRES_HARDWARE_TEST` |
| Decommit while preserving reservation | `sceKernelMunmap` model | `REQUIRES_HARDWARE_TEST` |
| Pagefile/shared mapping primitives | Needed for mapping foundation | `REQUIRES_HARDWARE_TEST` |
| Real file-backed guest mapping | Current kernel uses Win32/CRT handles | **NOT IMPLEMENTED** |
| Guard pages | No current upstream guest-memory requirement found | NOT REQUIRED BY CURRENT MODEL |

## Executable-memory / M7 blocker

This is intentionally not solved in M6.

Current upstream still relies on writable+executable memory:

- `ELFLoader.cpp` sets load segments to `PAGE_EXECUTE_READWRITE`;
- `CodePatcher.cpp` commits patch code as `PAGE_EXECUTE_READWRITE`;
- patch code must also be allocated within ±2 GiB of the original instruction for a relative jump.

The UWP application-facing memory API has materially different constraints: `VirtualAllocFromApp` does not create executable pages, while `VirtualProtectFromApp` can make pages executable for a `codeGeneration` app but rejects simultaneous write+execute protection. Therefore the current upstream RWX strategy cannot simply be copied into the UWP port.

Possible M7 investigation directions, **not implemented here**, include a strict write-then-execute lifecycle and a patch-code allocator that can satisfy the ±2 GiB placement rule without RWX. Guest TLS/ABI/native execution must be validated separately before selecting a strategy.

## Dirty tracking

The Vulkan backend currently uses host page protection plus a vectored exception handler to detect writes into cached guest buffers. M6 does not migrate or redesign this mechanism because:

- D3D12 is not yet the PS4 GPU renderer;
- host exception behavior needs target validation;
- this belongs with later GPU/memory integration, not the initial guest reservation primitive.

Status: **INVESTIGATE**.

## Validation record

### Static diagnostic validation

Command:

```text
python3 platform/xbox/tools/validate_guest_memory.py
```

Expected repository-side checks:

- exact M6 layout constants still match authoritative upstream;
- UWP reservation/commit/query/protection/decommit/mapping primitives exist;
- no executable-page implementation exists in M6;
- current upstream RWX dependency is still explicitly detected as an M7 risk;
- checked-in JSON is `NOT_RUN`.

### UWP build

Command to run on Windows:

```text
msbuild platform/xbox/XboxCapabilityProbe.sln /m /t:Rebuild /p:Configuration=Release /p:Platform=x64
```

Current environment: **NOT TESTED / ENVIRONMENT BLOCKED** because `msbuild` is unavailable.

### Xbox runtime

- exact 2 TiB reservation: **NOT TESTED**
- fixed guest base: **NOT TESTED**
- data commit/decommit/protection: **NOT TESTED**
- shared mapping: **NOT TESTED**
- executable guest memory: **NOT TESTED — M7**
- guest/native execution: **NOT TESTED — M7**

## Files added/changed by M6

- `platform/xbox/Memory/XboxGuestMemory.hpp`
- `platform/xbox/Memory/XboxGuestMemory.cpp`
- `platform/xbox/XboxCapabilityProbe/MainPage.xaml`
- `platform/xbox/XboxCapabilityProbe/MainPage.xaml.h`
- `platform/xbox/XboxCapabilityProbe/MainPage.xaml.cpp`
- `platform/xbox/XboxCapabilityProbe/XboxCapabilityProbe.vcxproj`
- `platform/xbox/XboxCapabilityProbe/XboxCapabilityProbe.vcxproj.filters`
- `platform/xbox/tools/validate_guest_memory.py`
- `xbox-guest-memory.json`
- `docs/XBOX_GUEST_MEMORY.md`

## M6 exit status

**PARTIAL** — the exact current-upstream data-memory foundation and diagnostic are implemented without modifying emulator logic, but the required Xbox runtime measurements are unavailable. Executable/native guest memory is deliberately deferred to M7, where the upstream RWX assumption must be reconciled with UWP's write/execute restrictions only after hardware capability results are collected.
