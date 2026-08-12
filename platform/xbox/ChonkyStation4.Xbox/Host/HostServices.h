#pragma once

namespace ChonkyStation4
{
namespace Xbox
{

class HostServices
{
public:
    HostServices() = default;
    ~HostServices() = default;

    bool Initialize();
    void Shutdown();
};

}
}

