# ChonkyStation4 Xbox Port Changelog

## 2026-08-12 - Xbox Dev Mode/UWP delivery

- Fixed the existing Xbox host project XML and build configuration for the installed VS 2026/v143 toolchain.
- Added stable XAML implementation glue and writable generated-file routing for clean builds.
- Implemented the D3D12 host test backend with adapter/device creation, composition swap chain, XAML binding, synchronization, clear, triangle, and textured-quad scenes.
- Completed host lifecycle, storage, logging, controller visibility, configuration/runtime reporting, and package metadata.
- Added the missing XboxCapabilityProbe project to the existing solution set.
- Corrected the probe App.xaml namespace and generated-header wiring.
- Replaced the guest-memory UI placeholder with the existing real XboxGuestMemory diagnostic and JSON output.
- Added MSIX PhoneIdentity metadata to both Xbox manifests.
- Passed project, D3D12, capability-probe, and guest-memory validators.
- Produced x64 Release unsigned MSIX packages for both the host and capability probe.

## Known limitations

- No physical Xbox Dev Mode device was connected in the build environment.
- Full PS4 CPU execution, complete PS4 services, and game compatibility are not part of this port milestone.
