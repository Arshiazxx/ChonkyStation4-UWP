#pragma once

#include "MainPage.generated.g.h"

namespace XboxCapabilityProbe
{

public ref class MainPage sealed
{
public:
    MainPage();

private:
    void OnRunProbe(
        Platform::Object^ sender,
        Windows::UI::Xaml::RoutedEventArgs^ e);

    void OnRunGuestMemory(
        Platform::Object^ sender,
        Windows::UI::Xaml::RoutedEventArgs^ e);
};

}
