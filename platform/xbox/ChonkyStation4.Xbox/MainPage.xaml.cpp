#include "pch.h"
#include "MainPage.xaml.h"

#include "MainPage.XamlGlue.hpp"
#include "Host/HostServices.hpp"
#include "Core/Loader/Elf64Loader.hpp"
#include "Core/CPU/CpuSmokeTests.hpp"
#include "Core/Kernel/RuntimeSmokeTests.hpp"
#include "Core/ABI/AbiSmokeTests.hpp"
#include "Core/Execution/ModuleSmokeTests.hpp"
#include "Core/Execution/DynamicModuleSmokeTests.hpp"

using namespace ChonkyStation4::Xbox;

namespace {
std::wstring WithPrefix(const wchar_t* prefix, const std::wstring& status) {
    std::wstring message(prefix);
    message += L": ";
    message += status;
    return message;
}

std::string NarrowUtf8(const std::wstring& value) {
    if (value.empty()) {
        return {};
    }
    const int required = WideCharToMultiByte(
        CP_UTF8, WC_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()),
        nullptr, 0, nullptr, nullptr);
    if (required <= 0) {
        std::string result;
        result.reserve(value.size());
        for (const auto character : value) {
            result.push_back(character <= 0x7f ? static_cast<char>(character) : '?');
        }
        return result;
    }
    std::string result(static_cast<std::size_t>(required), '\0');
    WideCharToMultiByte(
        CP_UTF8, WC_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()),
        result.data(), required, nullptr, nullptr);
    return result;
}

std::wstring WidenUtf8(const std::string& value) {
    if (value.empty()) {
        return {};
    }
    const int required = MultiByteToWideChar(
        CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()),
        nullptr, 0);
    if (required <= 0) {
        return std::wstring(value.begin(), value.end());
    }
    std::wstring result(static_cast<std::size_t>(required), L'\0');
    MultiByteToWideChar(
        CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()),
        result.data(), required);
    return result;
}
}

MainPage::MainPage()
    : graphics_(std::make_unique<Graphics::D3D12Backend>()) {
    InitializeComponent();

    RuntimeText->Text = ref new Platform::String(WithPrefix(L"Runtime device family", Host::RuntimeDeviceFamily()->Data()).c_str());
    StorageText->Text = ref new Platform::String(WithPrefix(L"Local storage", Host::LocalStoragePath()->Data()).c_str());
    RefreshControllerText();

    const auto panel = reinterpret_cast<IUnknown*>(GraphicsPanel);
    if (graphics_->Initialize(panel, 1280, 720)) {
        StatusText->Text = ref new Platform::String(graphics_->Status().c_str());
        Host::LogString(StatusText->Text);
    } else {
        const auto status = std::wstring(L"D3D12 startup failed: ") + graphics_->Status();
        StatusText->Text = ref new Platform::String(status.c_str());
        Host::LogString(StatusText->Text);
    }
}

MainPage::~MainPage() {
    if (graphics_) {
        graphics_->Shutdown();
    }
}

void MainPage::OnPageLoaded(
    Platform::Object^,
    Windows::UI::Xaml::RoutedEventArgs^) {
    RefreshControllerText();
    if (graphics_ && graphics_->IsInitialized()) {
        Render(Graphics::DemoScene::Clear, L"Initial frame");
    }
}

void MainPage::RefreshControllerText() {
    const auto count = Windows::Gaming::Input::Gamepad::Gamepads->Size;
    const std::wstring text = L"Controllers visible: " + std::to_wstring(count);
    GamepadText->Text = ref new Platform::String(text.c_str());
}

void MainPage::Render(Graphics::DemoScene scene, const wchar_t* label) {
    RefreshControllerText();
    if (!graphics_ || !graphics_->IsInitialized()) {
        StatusText->Text = ref new Platform::String(
            WithPrefix(label, graphics_ ? graphics_->Status() : L"backend unavailable").c_str());
        return;
    }
    const bool rendered = graphics_->Render(scene);
    std::wstring status = WithPrefix(label, graphics_->Status());
    if (!rendered) {
        status = L"Render failed: " + status;
    }
    StatusText->Text = ref new Platform::String(status.c_str());
    Host::LogString(StatusText->Text);
}

void MainPage::OnRenderClear(
    Platform::Object^,
    Windows::UI::Xaml::RoutedEventArgs^) {
    Render(Graphics::DemoScene::Clear, L"Clear frame");
}

void MainPage::OnRenderTriangle(
    Platform::Object^,
    Windows::UI::Xaml::RoutedEventArgs^) {
    Render(Graphics::DemoScene::Triangle, L"Triangle frame");
}

void MainPage::OnRenderTexturedQuad(
    Platform::Object^,
    Windows::UI::Xaml::RoutedEventArgs^) {
    Render(Graphics::DemoScene::TexturedQuad, L"Textured quad frame");
}

void MainPage::OnLoadTestElf(
    Platform::Object^,
    Windows::UI::Xaml::RoutedEventArgs^) {
    std::wstring path = Host::LocalStoragePath()->Data();
    if (!path.empty() && path.back() != L'\\' && path.back() != L'/') {
        path += L'\\';
    }
    path += L"eboot.bin";

    Core::Loader::Elf64Loader loader;
    const auto report = loader.LoadFile(NarrowUtf8(path));
    const auto reportText = report.ToText();
    const auto wideReport = WidenUtf8(reportText);
    StatusText->Text = ref new Platform::String(wideReport.c_str());
    Host::Log(wideReport.c_str());
}

void MainPage::OnRunCpuTest(
    Platform::Object^,
    Windows::UI::Xaml::RoutedEventArgs^) {
    const auto report = Core::CPU::RunCpuSmokeTests();
    const auto wideReport = WidenUtf8(report.log);
    StatusText->Text = ref new Platform::String(wideReport.c_str());
    Host::Log(wideReport.c_str());
}

void MainPage::OnRunRuntimeTest(
    Platform::Object^,
    Windows::UI::Xaml::RoutedEventArgs^) {
    const auto report = Core::Kernel::RunRuntimeSmokeTests();
    const std::string output = "ChonkyStation4 Runtime Test\n\n" + report.log;
    const auto wideReport = WidenUtf8(output);
    StatusText->Text = ref new Platform::String(wideReport.c_str());
    Host::Log(wideReport.c_str());
}

void MainPage::OnRunAbiTest(
    Platform::Object^,
    Windows::UI::Xaml::RoutedEventArgs^) {
    const auto report = Core::ABI::RunAbiSmokeTests();
    const auto wideReport = WidenUtf8(report.log);
    StatusText->Text = ref new Platform::String(wideReport.c_str());
    Host::Log(wideReport.c_str());
}

void MainPage::OnRunModuleTest(
    Platform::Object^,
    Windows::UI::Xaml::RoutedEventArgs^) {
    const auto report = Core::Execution::RunModuleSmokeTests();
    const auto wideReport = WidenUtf8(report.log);
    StatusText->Text = ref new Platform::String(wideReport.c_str());
    Host::Log(wideReport.c_str());
}

void MainPage::OnRunDependencyTest(
    Platform::Object^,
    Windows::UI::Xaml::RoutedEventArgs^) {
    const auto report = Core::Execution::RunDynamicModuleSmokeTests();
    const auto wideReport = WidenUtf8(report.log);
    StatusText->Text = ref new Platform::String(wideReport.c_str());
    Host::Log(wideReport.c_str());
}
