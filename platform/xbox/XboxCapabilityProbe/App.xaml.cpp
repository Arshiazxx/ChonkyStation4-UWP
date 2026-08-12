#include "pch.h"
#include "App.xaml.h"
#include "App.XamlGlue.hpp"

using namespace Windows::ApplicationModel;
using namespace Windows::ApplicationModel::Activation;
using namespace Windows::UI::Xaml;
using namespace Windows::UI::Xaml::Controls;
using namespace Windows::UI::Xaml::Navigation;

namespace XboxCapabilityProbe {

App::App() {
    InitializeComponent();
    Suspending += ref new SuspendingEventHandler(this, &App::OnSuspending);
}

void App::OnLaunched(LaunchActivatedEventArgs^ e) {
    auto rootFrame = dynamic_cast<Frame^>(Window::Current->Content);
    if (rootFrame == nullptr) {
        rootFrame = ref new Frame();
        rootFrame->NavigationFailed += ref new NavigationFailedEventHandler(this, &App::OnNavigationFailed);
        Window::Current->Content = rootFrame;
    }
    if (rootFrame->Content == nullptr) {
        rootFrame->Navigate(Windows::UI::Xaml::Interop::TypeName(MainPage::typeid), e->Arguments);
    }
    Window::Current->Activate();
}

void App::OnSuspending(Platform::Object^, SuspendingEventArgs^ e) {
    auto deferral = e->SuspendingOperation->GetDeferral();
    deferral->Complete();
}

void App::OnNavigationFailed(Platform::Object^, NavigationFailedEventArgs^ e) {
    std::wstring message = L"Failed to load page ";
    message += e->SourcePageType.Name->Data();
    throw ref new Platform::FailureException(ref new Platform::String(message.c_str()));
}

} // namespace XboxCapabilityProbe



