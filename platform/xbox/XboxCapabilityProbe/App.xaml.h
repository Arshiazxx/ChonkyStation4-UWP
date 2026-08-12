#pragma once

#include "App.generated.g.h"

namespace XboxCapabilityProbe
{

ref class App sealed
{
public:
    App();

protected:
    virtual void OnLaunched(
        Windows::ApplicationModel::Activation::LaunchActivatedEventArgs^ e) override;

private:
    void OnSuspending(
        Platform::Object^ sender,
        Windows::ApplicationModel::SuspendingEventArgs^ e);

    void OnNavigationFailed(
        Platform::Object^ sender,
        Windows::UI::Xaml::Navigation::NavigationFailedEventArgs^ e);
};

}
