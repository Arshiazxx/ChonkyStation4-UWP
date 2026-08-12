#pragma once

#include "App.g.h"

namespace ChonkyStation4
{
namespace Xbox
{

public ref class App sealed
{
public:
    App();

protected:
    virtual void OnLaunched(
        Windows::ApplicationModel::Activation::LaunchActivatedEventArgs^ args
    ) override;

private:
    void OnSuspending(
        Platform::Object^ sender,
        Windows::ApplicationModel::SuspendingEventArgs^ e
    );

    void OnNavigationFailed(
        Platform::Object^ sender,
        Windows::UI::Xaml::Navigation::NavigationFailedEventArgs^ e
    );
};

}

}
