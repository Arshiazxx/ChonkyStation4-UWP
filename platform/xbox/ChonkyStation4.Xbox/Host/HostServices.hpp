#pragma once

namespace ChonkyStation4 {
namespace Xbox {
namespace Host {

Platform::String^ PackageVersion();
Platform::String^ BuildConfiguration();
Platform::String^ RuntimeDeviceFamily();
Platform::String^ LocalStoragePath();
void LogString(Platform::String^ message);
void Log(const wchar_t* message);

} // namespace Host
} // namespace Xbox
} // namespace ChonkyStation4

