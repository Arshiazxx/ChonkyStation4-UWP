# ChonkyStation4 Xbox Port - Final Delivery

Date: 2026-08-12

## Delivered

This delivery preserves the existing port and adds a buildable Xbox Dev Mode/UWP host path. The host and capability-probe projects both build as x64 Release UWP applications and package as unsigned MSIX files.

## Host features

- UWP/Xbox application lifecycle with launch, suspension, navigation-failure, and logging hooks.
- LocalFolder-based storage path reporting.
- Xbox/gamepad visibility reporting through Windows.Gaming.Input.
- D3D12 adapter selection, device/queue/allocator/list creation, fence synchronization, RTV/SRV heaps, composition swap chain, and XAML SwapChainPanel binding.
- Runtime HLSL pipeline setup for clear, triangle, and textured-quad test scenes.
- Package manifest metadata required by MSIX, including Windows.Xbox target family and PhoneIdentity.
- Stable checked-in XAML implementation glue for clean builds with the installed VS 2026 XAML target.

## Probe features

- Host capability report written to LocalFolder as `xbox-capabilities.json`.
- Real data-memory diagnostic wired to the UI and written as `xbox-guest-memory.json`.
- Exact-base reservation, commit, read/write, protection, decommit, shared mapping, and release checks.
- No executable guest pages or guest execution are claimed or enabled by the probe.

## Verified commands

The builds used the installed VS 2026 toolchain with the v143 C++ toolset:

```text
call E:\VS Studio\Common7\Tools\VsDevCmd.bat -arch=x64 -host_arch=x64 -vcvars_ver=14.44
MSBuild platform\xbox\ChonkyStation4.Xbox\ChonkyStation4.Xbox.vcxproj /t:Rebuild /p:Configuration=Release /p:Platform=x64
MSBuild platform\xbox\XboxCapabilityProbe\XboxCapabilityProbe.vcxproj /t:Rebuild /p:Configuration=Release /p:Platform=x64
```

Both commands completed with zero compiler, linker, or packaging errors. The generated package outputs are under `build-out\XboxHost` and `build-out\XboxProbe`.

## Installation

Deploy the x64 MSIX from the relevant `AppPackages\\*_x64_Test` directory to an Xbox running Dev Mode using the standard Xbox Device Portal/app deployment flow. The packages are unsigned test packages; Dev Mode deployment must accept/install the package certificate as appropriate for the device.

## Limitations

- This environment has no connected Xbox Dev Mode console, so physical-device installation and launch were not independently exercised here.
- The existing upstream core remains native x86-64 guest execution; a portable interpreter/JIT vertical slice is intentionally not fabricated.
- The host currently validates the graphics test-frame path; full PS4 game execution, complete kernel/service coverage, and production shader translation remain future work.
