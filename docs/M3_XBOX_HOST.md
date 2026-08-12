# M3 — Xbox UWP Host

Upstream source of truth: `liuk7071/ChonkyStation4` at `310269290a3c256f5911d4bc7e441489bffffbf6`.

## Status

**PARTIAL**.

The UWP/Xbox host source, project, manifest, diagnostic UI, lifecycle hooks, controller foundation, storage foundation, and validation tooling are present. This environment cannot perform the required Windows/UWP build because it has no MSBuild, MSVC, Windows SDK, or AppX/MSIX packaging toolchain. No Xbox hardware execution is claimed.

## Project structure

```text
platform/xbox/
├── ChonkyStation4.Xbox.sln
├── tools/
│   └── validate_project.py
└── ChonkyStation4.Xbox/
    ├── App.xaml
    ├── App.xaml.cpp
    ├── App.xaml.h
    ├── MainPage.xaml
    ├── MainPage.xaml.cpp
    ├── MainPage.xaml.h
    ├── pch.cpp
    ├── pch.h
    ├── Host/
    │   ├── HostServices.cpp
    │   └── HostServices.hpp
    ├── Assets/
    ├── ChonkyStation4.Xbox.vcxproj
    ├── ChonkyStation4.Xbox.vcxproj.filters
    └── Package.appxmanifest
```

## Host scope

M3 deliberately does **not** link or start the emulator core. It is a host shell only.

The intended first runtime statement is therefore:

```text
ChonkyStation4 Xbox UWP host starts
```

not:

```text
PS4 emulation works on Xbox
```

## UWP integration

The host is an AppContainer UWP application with:

- x64 Debug and Release configurations;
- `Windows Store` application type;
- Windows SDK target `10.0.26100.0`;
- minimum SDK `10.0.17763.0`;
- `Windows.Xbox` target device family in the package manifest;
- C++17 host code with WinRT/XAML support;
- unsigned packaging configuration by default so repository builds do not depend on a committed private certificate.

The project remains separate from the upstream desktop CMake target. No UWP-specific settings are injected into upstream emulator source.

## Lifecycle

`App.xaml.cpp` handles:

- application construction;
- launch and main-page navigation;
- activation;
- suspension deferral and shutdown-safe host logging;
- navigation failure.

No emulator worker thread is started in M3.

## Diagnostic UI

`MainPage.xaml` displays:

- project name;
- package version;
- build configuration;
- **actual WinRT runtime device family** (`AnalyticsInfo::VersionInfo->DeviceFamily`);
- host initialization state;
- graphics-backend state;
- capability-probe state;
- connected gamepad count;
- UWP local-storage path.

The graphics field explicitly states that D3D12 is not initialized until M4. The capability field explicitly states that M5 has not run.

A `SwapChainPanel` is present as the future graphics host surface. M3 does not attach a swap chain to it.

## Logging

`Host/HostServices.cpp` provides a minimal Xbox/UWP host logger using the Windows debug-output channel. This is used by the actual App/MainPage code and is therefore not an unused abstraction.

It is intentionally separate from the emulator's logging implementation until the host and emulator are connected in a later milestone.

## Controller input foundation

The diagnostic page subscribes to WinRT `Windows::Gaming::Input::Gamepad` added/removed events and displays the number of currently visible gamepads.

This is host/controller discovery only. It is not yet wired into `scePad` guest semantics.

## Filesystem/storage foundation

The host resolves and displays `Windows::Storage::ApplicationData::Current->LocalFolder`.

This establishes an AppContainer-safe writable storage root without changing `PS4::FS` guest path behavior. Guest filesystem integration is intentionally deferred until an actual host/core bridge is introduced.

## Graphics initialization path

M3 reserves a XAML `SwapChainPanel` named `GraphicsPanel` and exposes graphics status separately from application initialization.

M4 attaches the D3D12 foundation to this surface. No Vulkan or SDL object is required by the UWP shell.

## Manifest capabilities

The M3 manifest requests only `internetClient`. It does not request broad filesystem capability or claim capabilities that are not required by this shell.

No `PhoneIdentity` workaround is present.

## Validation

### Static project validation

Command:

```bash
python3 platform/xbox/tools/validate_project.py
```

Result: **PASS**.

Validated:

- project XML parses;
- manifest XML parses;
- every source/XAML/asset reference exists;
- `AppContainerApplication=true`;
- `ApplicationType=Windows Store`;
- target family is `Windows.Xbox`;
- declared Xbox min/max target versions are present.

### UWP build

Intended Windows command:

```powershell
msbuild platform\xbox\ChonkyStation4.Xbox.sln `
  /m /t:Rebuild `
  /p:Configuration=Release `
  /p:Platform=x64
```

Attempt in the current environment: **BLOCKED**.

Observed result:

```text
msbuild: command not found
exit=127
```

The container is Linux and contains no MSBuild/MSVC/Windows SDK/AppX toolchain. This is an environment blocker, not recorded as a successful host build.

### MSIX/AppX package

**NOT TESTED / NOT PRODUCED** because the Windows packaging toolchain is unavailable.

### Runtime

- Windows desktop UWP launch: **NOT TESTED**.
- Xbox Dev Mode launch: **NOT TESTED**.
- Controller events on Xbox hardware: **NOT TESTED**.

## Files changed

All M3 additions are isolated under `platform/xbox` plus this document.

No upstream emulator implementation file is modified by M3.

## Known limitations

1. Windows build validation must still be performed with Visual Studio/MSBuild and Windows SDK 10.0.26100.0 or a compatible installed SDK.
2. Package signing/deployment credentials are intentionally not committed.
3. Graphics output is not initialized until M4.
4. The host is not yet connected to PS4 loader/kernel/GCN execution.
5. Input is discovery/host UI only, not guest `scePad` translation.

## Milestone ledger

- Implementation: **PASS by source/static validation**
- UWP compile/link: **NOT TESTED — toolchain unavailable**
- Package generation: **NOT TESTED**
- Runtime launch: **NOT TESTED**
- Overall M3 status: **PARTIAL**
