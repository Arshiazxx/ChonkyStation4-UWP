#include "pch.h"
#include "HostServices.hpp"

using namespace Windows::ApplicationModel;
using namespace Windows::Storage;
using namespace Windows::System::Profile;

namespace ChonkyStation4::Xbox::Host {

Platform::String^ PackageVersion() {
    const auto version = Package::Current->Id->Version;
    std::wostringstream text;
    text << version.Major << L"." << version.Minor << L"."
         << version.Build << L"." << version.Revision;
    return ref new Platform::String(text.str().c_str());
}

Platform::String^ BuildConfiguration() {
#ifdef NDEBUG
    return L"Release";
#else
    return L"Debug";
#endif
}

Platform::String^ RuntimeDeviceFamily() {
    return AnalyticsInfo::VersionInfo->DeviceFamily;
}

Platform::String^ LocalStoragePath() {
    return ApplicationData::Current->LocalFolder->Path;
}

void LogString(Platform::String^ message) {
    if (message != nullptr) {
        OutputDebugStringW(message->Data());
        OutputDebugStringW(L"\n");
    }
}

void Log(const wchar_t* message) {
    OutputDebugStringW(message ? message : L"(null)");
    OutputDebugStringW(L"\n");
}

} // namespace ChonkyStation4::Xbox::Host
