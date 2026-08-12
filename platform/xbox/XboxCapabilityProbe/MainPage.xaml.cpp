#include "pch.h"
#include "MainPage.xaml.h"
#include "MainPage.XamlGlue.hpp"
#include "Probe/CapabilityProbe.hpp"
#include "../Memory/XboxGuestMemory.hpp"
#include <windows.storage.h>
namespace XboxCapabilityProbe {

MainPage::MainPage() {
    InitializeComponent();
    OutputText->Text = L"Press 'Run capability probe'. Results are written to xbox-capabilities.json in the app LocalFolder.\nNo generated guest/JIT code is executed by M5.";
}

void MainPage::OnRunProbe(Platform::Object^, Windows::UI::Xaml::RoutedEventArgs^) {
    RunButton->IsEnabled = false;
    StatusText->Text = L"Running...";

    try {
        const auto report = Probe::CapabilityProbe::Run();
        const auto outputPath = Probe::CapabilityProbe::WriteJson(report);
        OutputText->Text = ref new Platform::String(report.ToSummary().c_str());
        const std::wstring completed = L"Complete: " + std::wstring(outputPath->Data());
        StatusText->Text = ref new Platform::String(completed.c_str());
    }
    catch (Platform::Exception^ e) {
        StatusText->Text = L"Probe failed";
        OutputText->Text = e->Message;
    }
    catch (const std::exception& e) {
        StatusText->Text = L"Probe failed";
        std::wstring message(e.what(), e.what() + std::strlen(e.what()));
        OutputText->Text = ref new Platform::String(message.c_str());
    }

    RunButton->IsEnabled = true;
}

void MainPage::OnRunGuestMemory(Platform::Object^, Windows::UI::Xaml::RoutedEventArgs^) {
    GuestMemoryButton->IsEnabled = false;
    StatusText->Text = L"Running guest memory probe...";

    try {
        const auto report = ChonkyStation4::Xbox::Memory::RunGuestMemoryDiagnostic();
        const std::wstring summary = report.ToSummary();
        OutputText->Text = ref new Platform::String(summary.c_str());

        std::wstring path(Windows::Storage::ApplicationData::Current->LocalFolder->Path->Data());
        path += L"\\xbox-guest-memory.json";
        const auto json = report.ToJson();
        std::ofstream out(path, std::ios::binary | std::ios::trunc);
        out.write(json.data(), static_cast<std::streamsize>(json.size()));
        out.close();
        if (!out) {
            throw ref new Platform::FailureException(L"Failed to write xbox-guest-memory.json");
        }

        const std::wstring completed = report.CoreDataMemoryRequirementsPassed()
            ? L"Complete: core data-memory requirements passed"
            : L"Complete: one or more core data-memory checks failed";
        StatusText->Text = ref new Platform::String(completed.c_str());
    }
    catch (Platform::Exception^ e) {
        StatusText->Text = L"Guest-memory diagnostic failed";
        OutputText->Text = e->Message;
    }

    GuestMemoryButton->IsEnabled = true;
}
} // namespace XboxCapabilityProbe




