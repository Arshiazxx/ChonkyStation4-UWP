#pragma once

namespace ChonkyStation4
{
namespace Xbox
{

class D3D12Backend
{
public:
    D3D12Backend() = default;
    ~D3D12Backend() = default;

    bool Initialize();
    void Resize(unsigned int width, unsigned int height);
    void Shutdown();
};

}
}

