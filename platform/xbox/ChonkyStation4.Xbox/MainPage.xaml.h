#pragma once

#include "MainPage.g.h"

#include <memory>

#include "Graphics/D3D12Backend.hpp"

namespace ChonkyStation4::Xbox {

public ref class MainPage sealed {
public:
    MainPage();
    virtual ~MainPage();

private:
    void OnPageLoaded(
        Platform::Object^ sender,
        Windows::UI::Xaml::RoutedEventArgs^ e);
    void OnRenderClear(
        Platform::Object^ sender,
        Windows::UI::Xaml::RoutedEventArgs^ e);
    void OnRenderTriangle(
        Platform::Object^ sender,
        Windows::UI::Xaml::RoutedEventArgs^ e);
    void OnRenderTexturedQuad(
        Platform::Object^ sender,
        Windows::UI::Xaml::RoutedEventArgs^ e);
    void OnLoadTestElf(
        Platform::Object^ sender,
        Windows::UI::Xaml::RoutedEventArgs^ e);
    void OnRunCpuTest(
        Platform::Object^ sender,
        Windows::UI::Xaml::RoutedEventArgs^ e);
    void OnRunRuntimeTest(
        Platform::Object^ sender,
        Windows::UI::Xaml::RoutedEventArgs^ e);
    void OnRunAbiTest(
        Platform::Object^ sender,
        Windows::UI::Xaml::RoutedEventArgs^ e);
    void OnRunModuleTest(
        Platform::Object^ sender,
        Windows::UI::Xaml::RoutedEventArgs^ e);
    void OnRunDependencyTest(
        Platform::Object^ sender,
        Windows::UI::Xaml::RoutedEventArgs^ e);
    void OnRunUpstreamExecutionTest(
        Platform::Object^ sender,
        Windows::UI::Xaml::RoutedEventArgs^ e);

    void Render(Graphics::DemoScene scene, const wchar_t* label);
    void RefreshControllerText();

    std::unique_ptr<Graphics::D3D12Backend> graphics_;
};

} // namespace ChonkyStation4::Xbox
