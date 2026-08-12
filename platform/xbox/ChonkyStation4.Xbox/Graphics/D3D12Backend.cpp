#include "pch.h"
#include "D3D12Backend.hpp"
#include <d3d12.h>
#include <dxgi1_6.h>
#include <d3dcompiler.h>
#include <windows.ui.xaml.media.dxinterop.h>

#include <array>
#include <cstddef>
#include <cstring>
#include <limits>
#include <sstream>
#include <utility>

namespace ChonkyStation4::Xbox::Graphics {
namespace {
using Microsoft::WRL::ComPtr;

constexpr UINT kFrameCount = 2;
constexpr DXGI_FORMAT kBackBufferFormat = DXGI_FORMAT_B8G8R8A8_UNORM;

struct Vertex final {
    float position[3];
    float color[4];
    float uv[2];
};

std::wstring HrMessage(const wchar_t* operation, HRESULT hr) {
    std::wostringstream out;
    out << operation << L" failed (HRESULT 0x" << std::hex
        << static_cast<unsigned long>(hr) << L")";
    return out.str();
}

D3D12_HEAP_PROPERTIES HeapProperties(D3D12_HEAP_TYPE type) {
    D3D12_HEAP_PROPERTIES properties{};
    properties.Type = type;
    properties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    properties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
    properties.CreationNodeMask = 1;
    properties.VisibleNodeMask = 1;
    return properties;
}

D3D12_RESOURCE_DESC BufferDesc(UINT64 size) {
    D3D12_RESOURCE_DESC desc{};
    desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    desc.Alignment = 0;
    desc.Width = size;
    desc.Height = 1;
    desc.DepthOrArraySize = 1;
    desc.MipLevels = 1;
    desc.Format = DXGI_FORMAT_UNKNOWN;
    desc.SampleDesc.Count = 1;
    desc.SampleDesc.Quality = 0;
    desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    desc.Flags = D3D12_RESOURCE_FLAG_NONE;
    return desc;
}

D3D12_RESOURCE_BARRIER Transition(
    ID3D12Resource* resource,
    D3D12_RESOURCE_STATES before,
    D3D12_RESOURCE_STATES after) {
    D3D12_RESOURCE_BARRIER barrier{};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrier.Transition.pResource = resource;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    barrier.Transition.StateBefore = before;
    barrier.Transition.StateAfter = after;
    return barrier;
}

D3D12_BLEND_DESC OpaqueBlend() {
    D3D12_BLEND_DESC blend{};
    blend.AlphaToCoverageEnable = FALSE;
    blend.IndependentBlendEnable = FALSE;
    D3D12_RENDER_TARGET_BLEND_DESC target{};
    target.BlendEnable = FALSE;
    target.LogicOpEnable = FALSE;
    target.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
    blend.RenderTarget[0] = target;
    return blend;
}

D3D12_RASTERIZER_DESC SolidRasterizer() {
    D3D12_RASTERIZER_DESC rasterizer{};
    rasterizer.FillMode = D3D12_FILL_MODE_SOLID;
    rasterizer.CullMode = D3D12_CULL_MODE_NONE;
    rasterizer.FrontCounterClockwise = FALSE;
    rasterizer.DepthBias = D3D12_DEFAULT_DEPTH_BIAS;
    rasterizer.DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
    rasterizer.SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
    rasterizer.DepthClipEnable = TRUE;
    rasterizer.MultisampleEnable = FALSE;
    rasterizer.AntialiasedLineEnable = FALSE;
    rasterizer.ForcedSampleCount = 0;
    rasterizer.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;
    return rasterizer;
}

D3D12_DEPTH_STENCIL_DESC DisabledDepth() {
    D3D12_DEPTH_STENCIL_DESC depth{};
    depth.DepthEnable = FALSE;
    depth.StencilEnable = FALSE;
    return depth;
}

} // namespace

struct D3D12Backend::Impl final {
    bool initialized = false;
    unsigned width = 0;
    unsigned height = 0;
    std::wstring status = L"Not initialized";

    ComPtr<ISwapChainPanelNative> panel;
    ComPtr<IDXGIFactory4> factory;
    ComPtr<IDXGIAdapter1> adapter;
    ComPtr<ID3D12Device> device;
    ComPtr<ID3D12CommandQueue> commandQueue;
    ComPtr<ID3D12CommandAllocator> commandAllocator;
    ComPtr<ID3D12GraphicsCommandList> commandList;
    ComPtr<IDXGISwapChain3> swapChain;
    ComPtr<ID3D12DescriptorHeap> rtvHeap;
    ComPtr<ID3D12DescriptorHeap> srvHeap;
    std::array<ComPtr<ID3D12Resource>, kFrameCount> renderTargets{};
    UINT rtvDescriptorSize = 0;
    UINT frameIndex = 0;

    ComPtr<ID3D12Fence> fence;
    UINT64 fenceValue = 0;
    HANDLE fenceEvent = nullptr;

    ComPtr<ID3D12RootSignature> rootSignature;
    ComPtr<ID3D12PipelineState> trianglePipeline;
    ComPtr<ID3D12PipelineState> texturedPipeline;
    ComPtr<ID3D12Resource> triangleBuffer;
    ComPtr<ID3D12Resource> quadBuffer;
    ComPtr<ID3D12Resource> texture;
    ComPtr<ID3D12Resource> textureUpload;
    D3D12_VERTEX_BUFFER_VIEW triangleView{};
    D3D12_VERTEX_BUFFER_VIEW quadView{};

    void Fail(const wchar_t* operation, HRESULT hr) {
        initialized = false;
        status = HrMessage(operation, hr);
    }

    bool WaitForGpu() {
        if (!commandQueue || !fence || !fenceEvent) {
            return false;
        }

        const UINT64 value = ++fenceValue;
        HRESULT hr = commandQueue->Signal(fence.Get(), value);
        if (FAILED(hr)) {
            Fail(L"ID3D12CommandQueue::Signal", hr);
            return false;
        }

        if (fence->GetCompletedValue() < value) {
            hr = fence->SetEventOnCompletion(value, fenceEvent);
            if (FAILED(hr)) {
                Fail(L"ID3D12Fence::SetEventOnCompletion", hr);
                return false;
            }
            if (WaitForSingleObjectEx(fenceEvent, INFINITE, FALSE) != WAIT_OBJECT_0) {
                status = L"GPU fence wait failed";
                initialized = false;
                return false;
            }
        }
        return true;
    }

    bool CreateUploadBuffer(const void* data, size_t size, ComPtr<ID3D12Resource>& buffer,
                            D3D12_VERTEX_BUFFER_VIEW& view, UINT stride) {
        const auto properties = HeapProperties(D3D12_HEAP_TYPE_UPLOAD);
        const auto desc = BufferDesc(static_cast<UINT64>(size));
        HRESULT hr = device->CreateCommittedResource(
            &properties,
            D3D12_HEAP_FLAG_NONE,
            &desc,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&buffer));
        if (FAILED(hr)) {
            Fail(L"CreateCommittedResource(upload buffer)", hr);
            return false;
        }

        void* mapped = nullptr;
        D3D12_RANGE readRange{0, 0};
        hr = buffer->Map(0, &readRange, &mapped);
        if (FAILED(hr)) {
            Fail(L"ID3D12Resource::Map", hr);
            return false;
        }
        std::memcpy(mapped, data, size);
        buffer->Unmap(0, nullptr);

        view.BufferLocation = buffer->GetGPUVirtualAddress();
        view.SizeInBytes = static_cast<UINT>(size);
        view.StrideInBytes = stride;
        return true;
    }

    bool CreateTexture() {
        D3D12_RESOURCE_DESC textureDesc{};
        textureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        textureDesc.Width = 2;
        textureDesc.Height = 2;
        textureDesc.DepthOrArraySize = 1;
        textureDesc.MipLevels = 1;
        textureDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        textureDesc.SampleDesc.Count = 1;
        textureDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;

        const auto defaultHeap = HeapProperties(D3D12_HEAP_TYPE_DEFAULT);
        HRESULT hr = device->CreateCommittedResource(
            &defaultHeap,
            D3D12_HEAP_FLAG_NONE,
            &textureDesc,
            D3D12_RESOURCE_STATE_COPY_DEST,
            nullptr,
            IID_PPV_ARGS(&texture));
        if (FAILED(hr)) {
            Fail(L"CreateCommittedResource(texture)", hr);
            return false;
        }

        D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint{};
        UINT rows = 0;
        UINT64 rowSize = 0;
        UINT64 uploadSize = 0;
        device->GetCopyableFootprints(
            &textureDesc, 0, 1, 0, &footprint, &rows, &rowSize, &uploadSize);

        const auto uploadHeap = HeapProperties(D3D12_HEAP_TYPE_UPLOAD);
        const auto uploadDesc = BufferDesc(uploadSize);
        hr = device->CreateCommittedResource(
            &uploadHeap,
            D3D12_HEAP_FLAG_NONE,
            &uploadDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&textureUpload));
        if (FAILED(hr)) {
            Fail(L"CreateCommittedResource(texture upload)", hr);
            return false;
        }

        static constexpr unsigned char pixels[] = {
            0xff, 0x20, 0x20, 0xff, 0x20, 0xff, 0x20, 0xff,
            0x20, 0x20, 0xff, 0xff, 0xff, 0xd0, 0x20, 0xff,
        };
        void* mapped = nullptr;
        D3D12_RANGE readRange{0, 0};
        hr = textureUpload->Map(0, &readRange, &mapped);
        if (FAILED(hr)) {
            Fail(L"ID3D12Resource::Map(texture upload)", hr);
            return false;
        }
        auto* destination = static_cast<unsigned char*>(mapped) + footprint.Offset;
        for (UINT row = 0; row < rows; ++row) {
            std::memcpy(destination + row * footprint.Footprint.RowPitch,
                        pixels + row * 2 * 4, static_cast<size_t>(rowSize));
        }
        textureUpload->Unmap(0, nullptr);

        hr = commandAllocator->Reset();
        if (FAILED(hr)) {
            Fail(L"ID3D12CommandAllocator::Reset(texture)", hr);
            return false;
        }
        hr = commandList->Reset(commandAllocator.Get(), nullptr);
        if (FAILED(hr)) {
            Fail(L"ID3D12GraphicsCommandList::Reset(texture)", hr);
            return false;
        }

        D3D12_TEXTURE_COPY_LOCATION source{};
        source.pResource = textureUpload.Get();
        source.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
        source.PlacedFootprint = footprint;
        D3D12_TEXTURE_COPY_LOCATION destinationLocation{};
        destinationLocation.pResource = texture.Get();
        destinationLocation.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        destinationLocation.SubresourceIndex = 0;
        commandList->CopyTextureRegion(&destinationLocation, 0, 0, 0, &source, nullptr);
        const auto barrier = Transition(
            texture.Get(), D3D12_RESOURCE_STATE_COPY_DEST,
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        commandList->ResourceBarrier(1, &barrier);
        hr = commandList->Close();
        if (FAILED(hr)) {
            Fail(L"ID3D12GraphicsCommandList::Close(texture)", hr);
            return false;
        }

        ID3D12CommandList* lists[] = {commandList.Get()};
        commandQueue->ExecuteCommandLists(1, lists);
        if (!WaitForGpu()) {
            return false;
        }

        D3D12_SHADER_RESOURCE_VIEW_DESC srv{};
        srv.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        srv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        srv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srv.Texture2D.MipLevels = 1;
        device->CreateShaderResourceView(texture.Get(), &srv, srvHeap->GetCPUDescriptorHandleForHeapStart());
        return true;
    }

    bool CreatePipelineStates() {
        const D3D12_DESCRIPTOR_RANGE descriptorRange{
            D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND};
        D3D12_ROOT_PARAMETER rootParameter{};
        rootParameter.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        rootParameter.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
        rootParameter.DescriptorTable.NumDescriptorRanges = 1;
        rootParameter.DescriptorTable.pDescriptorRanges = &descriptorRange;

        D3D12_STATIC_SAMPLER_DESC sampler{};
        sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
        sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        sampler.MipLODBias = 0.0f;
        sampler.MaxAnisotropy = 1;
        sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;
        sampler.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
        sampler.MinLOD = 0.0f;
        sampler.MaxLOD = D3D12_FLOAT32_MAX;
        sampler.ShaderRegister = 0;
        sampler.RegisterSpace = 0;
        sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

        D3D12_ROOT_SIGNATURE_DESC rootDesc{};
        rootDesc.NumParameters = 1;
        rootDesc.pParameters = &rootParameter;
        rootDesc.NumStaticSamplers = 1;
        rootDesc.pStaticSamplers = &sampler;
        rootDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

        ComPtr<ID3DBlob> serialized;
        ComPtr<ID3DBlob> errors;
        HRESULT hr = D3D12SerializeRootSignature(
            &rootDesc, D3D_ROOT_SIGNATURE_VERSION_1, &serialized, &errors);
        if (FAILED(hr)) {
            Fail(L"D3D12SerializeRootSignature", hr);
            return false;
        }
        hr = device->CreateRootSignature(
            0, serialized->GetBufferPointer(), serialized->GetBufferSize(),
            IID_PPV_ARGS(&rootSignature));
        if (FAILED(hr)) {
            Fail(L"ID3D12Device::CreateRootSignature", hr);
            return false;
        }

        static constexpr char triangleVs[] =
            "struct VSIn { float3 position : POSITION; float4 color : COLOR; };"
            "struct VSOut { float4 position : SV_POSITION; float4 color : COLOR; };"
            "VSOut main(VSIn input) { VSOut output; output.position=float4(input.position,1);"
            "output.color=input.color; return output; }";
        static constexpr char trianglePs[] =
            "float4 main(float4 color : COLOR) : SV_TARGET { return color; }";
        static constexpr char texturedVs[] =
            "struct VSIn { float3 position : POSITION; float2 uv : TEXCOORD; };"
            "struct VSOut { float4 position : SV_POSITION; float2 uv : TEXCOORD; };"
            "VSOut main(VSIn input) { VSOut output; output.position=float4(input.position,1);"
            "output.uv=input.uv; return output; }";
        static constexpr char texturedPs[] =
            "Texture2D texture0 : register(t0); SamplerState sampler0 : register(s0);"
            "float4 main(float2 uv : TEXCOORD) : SV_TARGET { return texture0.Sample(sampler0, uv); }";

        auto compile = [&](const char* source, const char* target, ComPtr<ID3DBlob>& bytecode) {
            ComPtr<ID3DBlob> compileErrors;
            HRESULT compileHr = D3DCompile(
                source, std::strlen(source), nullptr, nullptr, nullptr,
                "main", target, D3DCOMPILE_ENABLE_STRICTNESS, 0, &bytecode, &compileErrors);
            if (FAILED(compileHr)) {
                Fail(L"D3DCompile", compileHr);
                return false;
            }
            return true;
        };

        ComPtr<ID3DBlob> triangleVsBytecode;
        ComPtr<ID3DBlob> trianglePsBytecode;
        ComPtr<ID3DBlob> texturedVsBytecode;
        ComPtr<ID3DBlob> texturedPsBytecode;
        if (!compile(triangleVs, "vs_5_0", triangleVsBytecode) ||
            !compile(trianglePs, "ps_5_0", trianglePsBytecode) ||
            !compile(texturedVs, "vs_5_0", texturedVsBytecode) ||
            !compile(texturedPs, "ps_5_0", texturedPsBytecode)) {
            return false;
        }

        D3D12_GRAPHICS_PIPELINE_STATE_DESC base{};
        base.pRootSignature = rootSignature.Get();
        base.BlendState = OpaqueBlend();
        base.RasterizerState = SolidRasterizer();
        base.DepthStencilState = DisabledDepth();
        base.SampleMask = UINT_MAX;
        base.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        base.NumRenderTargets = 1;
        base.RTVFormats[0] = kBackBufferFormat;
        base.SampleDesc.Count = 1;

        const D3D12_INPUT_ELEMENT_DESC triangleInput[] = {
            {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(Vertex, position),
             D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
            {"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, offsetof(Vertex, color),
             D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        };
        base.InputLayout = {triangleInput, _countof(triangleInput)};
        base.VS = {triangleVsBytecode->GetBufferPointer(), triangleVsBytecode->GetBufferSize()};
        base.PS = {trianglePsBytecode->GetBufferPointer(), trianglePsBytecode->GetBufferSize()};
        hr = device->CreateGraphicsPipelineState(&base, IID_PPV_ARGS(&trianglePipeline));
        if (FAILED(hr)) {
            Fail(L"ID3D12Device::CreateGraphicsPipelineState(triangle)", hr);
            return false;
        }

        const D3D12_INPUT_ELEMENT_DESC quadInput[] = {
            {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(Vertex, position),
             D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
            {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof(Vertex, uv),
             D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        };
        base.InputLayout = {quadInput, _countof(quadInput)};
        base.VS = {texturedVsBytecode->GetBufferPointer(), texturedVsBytecode->GetBufferSize()};
        base.PS = {texturedPsBytecode->GetBufferPointer(), texturedPsBytecode->GetBufferSize()};
        hr = device->CreateGraphicsPipelineState(&base, IID_PPV_ARGS(&texturedPipeline));
        if (FAILED(hr)) {
            Fail(L"ID3D12Device::CreateGraphicsPipelineState(textured)", hr);
            return false;
        }
        return true;
    }
};

D3D12Backend::D3D12Backend() : impl_(std::make_unique<Impl>()) {}

D3D12Backend::~D3D12Backend() {
    Shutdown();
}

bool D3D12Backend::Initialize(IUnknown* swapChainPanel, unsigned width, unsigned height) {
    Shutdown();
    if (!swapChainPanel) {
        impl_->status = L"SwapChainPanel interop object is null";
        return false;
    }

    impl_->width = (std::max)(1u, width);
    impl_->height = (std::max)(1u, height);

    HRESULT hr = CreateDXGIFactory2(0, IID_PPV_ARGS(&impl_->factory));
    if (FAILED(hr)) {
        impl_->Fail(L"CreateDXGIFactory2", hr);
        return false;
    }

    for (UINT index = 0; ; ++index) {
        ComPtr<IDXGIAdapter1> candidate;
        hr = impl_->factory->EnumAdapters1(index, &candidate);
        if (hr == DXGI_ERROR_NOT_FOUND) {
            break;
        }
        if (FAILED(hr)) {
            impl_->Fail(L"IDXGIFactory4::EnumAdapters1", hr);
            return false;
        }

        DXGI_ADAPTER_DESC1 description{};
        candidate->GetDesc1(&description);
        if ((description.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) != 0) {
            continue;
        }

        ComPtr<ID3D12Device> device;
        if (SUCCEEDED(D3D12CreateDevice(
                candidate.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device)))) {
            impl_->adapter = candidate;
            impl_->device = device;
            break;
        }
    }

    if (!impl_->device) {
        impl_->status = L"No hardware D3D12 adapter accepted feature level 11_0";
        return false;
    }

    D3D12_COMMAND_QUEUE_DESC queueDesc{};
    queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    hr = impl_->device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&impl_->commandQueue));
    if (FAILED(hr)) {
        impl_->Fail(L"ID3D12Device::CreateCommandQueue", hr);
        return false;
    }
    hr = impl_->device->CreateCommandAllocator(
        D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&impl_->commandAllocator));
    if (FAILED(hr)) {
        impl_->Fail(L"ID3D12Device::CreateCommandAllocator", hr);
        return false;
    }
    hr = impl_->device->CreateCommandList(
        0, D3D12_COMMAND_LIST_TYPE_DIRECT, impl_->commandAllocator.Get(),
        nullptr, IID_PPV_ARGS(&impl_->commandList));
    if (FAILED(hr)) {
        impl_->Fail(L"ID3D12Device::CreateCommandList", hr);
        return false;
    }
    hr = impl_->commandList->Close();
    if (FAILED(hr)) {
        impl_->Fail(L"ID3D12GraphicsCommandList::Close", hr);
        return false;
    }

    hr = impl_->device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&impl_->fence));
    if (FAILED(hr)) {
        impl_->Fail(L"ID3D12Device::CreateFence", hr);
        return false;
    }
    impl_->fenceEvent = CreateEventEx(nullptr, nullptr, 0, EVENT_MODIFY_STATE | SYNCHRONIZE);
    if (!impl_->fenceEvent) {
        impl_->status = L"CreateEventEx for D3D12 fence failed";
        return false;
    }

    D3D12_DESCRIPTOR_HEAP_DESC rtvDesc{};
    rtvDesc.NumDescriptors = kFrameCount;
    rtvDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    hr = impl_->device->CreateDescriptorHeap(&rtvDesc, IID_PPV_ARGS(&impl_->rtvHeap));
    if (FAILED(hr)) {
        impl_->Fail(L"ID3D12Device::CreateDescriptorHeap(RTV)", hr);
        return false;
    }
    impl_->rtvDescriptorSize = impl_->device->GetDescriptorHandleIncrementSize(
        D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

    D3D12_DESCRIPTOR_HEAP_DESC srvDesc{};
    srvDesc.NumDescriptors = 1;
    srvDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    srvDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    hr = impl_->device->CreateDescriptorHeap(&srvDesc, IID_PPV_ARGS(&impl_->srvHeap));
    if (FAILED(hr)) {
        impl_->Fail(L"ID3D12Device::CreateDescriptorHeap(SRV)", hr);
        return false;
    }

    DXGI_SWAP_CHAIN_DESC1 swapChainDesc{};
    swapChainDesc.Width = impl_->width;
    swapChainDesc.Height = impl_->height;
    swapChainDesc.Format = kBackBufferFormat;
    swapChainDesc.Stereo = FALSE;
    swapChainDesc.SampleDesc.Count = 1;
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.BufferCount = kFrameCount;
    swapChainDesc.Scaling = DXGI_SCALING_STRETCH;
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
    swapChainDesc.AlphaMode = DXGI_ALPHA_MODE_IGNORE;
    ComPtr<IDXGISwapChain1> swapChain1;
    hr = impl_->factory->CreateSwapChainForComposition(
        impl_->commandQueue.Get(), &swapChainDesc, nullptr, &swapChain1);
    if (FAILED(hr)) {
        impl_->Fail(L"IDXGIFactory2::CreateSwapChainForComposition", hr);
        return false;
    }
    hr = swapChain1.As(&impl_->swapChain);
    if (FAILED(hr)) {
        impl_->Fail(L"IDXGISwapChain1::QueryInterface(IDXGISwapChain3)", hr);
        return false;
    }

    hr = swapChainPanel->QueryInterface(IID_PPV_ARGS(&impl_->panel));
    if (FAILED(hr)) {
        impl_->Fail(L"SwapChainPanel QueryInterface(ISwapChainPanelNative)", hr);
        return false;
    }
    // ISwapChainPanelNative::SetSwapChain binds the composition swap chain to XAML.
    hr = impl_->panel->SetSwapChain(impl_->swapChain.Get());
    if (FAILED(hr)) {
        impl_->Fail(L"ISwapChainPanelNative::SetSwapChain", hr);
        return false;
    }

    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle =
        impl_->rtvHeap->GetCPUDescriptorHandleForHeapStart();
    for (UINT index = 0; index < kFrameCount; ++index) {
        hr = impl_->swapChain->GetBuffer(index, IID_PPV_ARGS(&impl_->renderTargets[index]));
        if (FAILED(hr)) {
            impl_->Fail(L"IDXGISwapChain3::GetBuffer", hr);
            return false;
        }
        impl_->device->CreateRenderTargetView(
            impl_->renderTargets[index].Get(), nullptr, rtvHandle);
        rtvHandle.ptr += impl_->rtvDescriptorSize;
    }

    const Vertex triangle[] = {
        {{0.0f, 0.65f, 0.0f}, {1.0f, 0.2f, 0.2f, 1.0f}, {0.0f, 0.0f}},
        {{0.65f, -0.55f, 0.0f}, {0.2f, 1.0f, 0.3f, 1.0f}, {1.0f, 1.0f}},
        {{-0.65f, -0.55f, 0.0f}, {0.2f, 0.4f, 1.0f, 1.0f}, {0.0f, 1.0f}},
    };
    const Vertex quad[] = {
        {{-0.75f, 0.75f, 0.0f}, {}, {0.0f, 0.0f}},
        {{0.75f, 0.75f, 0.0f}, {}, {1.0f, 0.0f}},
        {{-0.75f, -0.75f, 0.0f}, {}, {0.0f, 1.0f}},
        {{-0.75f, -0.75f, 0.0f}, {}, {0.0f, 1.0f}},
        {{0.75f, 0.75f, 0.0f}, {}, {1.0f, 0.0f}},
        {{0.75f, -0.75f, 0.0f}, {}, {1.0f, 1.0f}},
    };
    if (!impl_->CreateUploadBuffer(
            triangle, sizeof(triangle), impl_->triangleBuffer, impl_->triangleView,
            sizeof(Vertex)) ||
        !impl_->CreateUploadBuffer(
            quad, sizeof(quad), impl_->quadBuffer, impl_->quadView, sizeof(Vertex)) ||
        !impl_->CreatePipelineStates()) {
        return false;
    }
    if (!impl_->CreateTexture()) {
        return false;
    }

    impl_->frameIndex = impl_->swapChain->GetCurrentBackBufferIndex();
    impl_->initialized = true;
    impl_->status = L"D3D12 initialized; clear/triangle/textured-quad diagnostics ready";
    return true;
}

void D3D12Backend::Shutdown() {
    if (!impl_) {
        return;
    }
    if (impl_->initialized) {
        impl_->WaitForGpu();
    }
    if (impl_->panel) {
        impl_->panel->SetSwapChain(nullptr);
    }
    if (impl_->fenceEvent) {
        CloseHandle(impl_->fenceEvent);
        impl_->fenceEvent = nullptr;
    }
    impl_->renderTargets = {};
    impl_->textureUpload.Reset();
    impl_->texture.Reset();
    impl_->quadBuffer.Reset();
    impl_->triangleBuffer.Reset();
    impl_->texturedPipeline.Reset();
    impl_->trianglePipeline.Reset();
    impl_->rootSignature.Reset();
    impl_->srvHeap.Reset();
    impl_->rtvHeap.Reset();
    impl_->swapChain.Reset();
    impl_->commandList.Reset();
    impl_->commandAllocator.Reset();
    impl_->commandQueue.Reset();
    impl_->fence.Reset();
    impl_->device.Reset();
    impl_->adapter.Reset();
    impl_->factory.Reset();
    impl_->panel.Reset();
    impl_->initialized = false;
    if (impl_->status.empty()) {
        impl_->status = L"Shutdown";
    }
}

bool D3D12Backend::Render(DemoScene scene) {
    if (!impl_->initialized) {
        return false;
    }

    impl_->frameIndex = impl_->swapChain->GetCurrentBackBufferIndex();
    HRESULT hr = impl_->commandAllocator->Reset();
    if (FAILED(hr)) {
        impl_->Fail(L"ID3D12CommandAllocator::Reset", hr);
        return false;
    }
    ID3D12PipelineState* pipeline =
        scene == DemoScene::TexturedQuad ? impl_->texturedPipeline.Get() :
        scene == DemoScene::Triangle ? impl_->trianglePipeline.Get() : nullptr;
    hr = impl_->commandList->Reset(impl_->commandAllocator.Get(), pipeline);
    if (FAILED(hr)) {
        impl_->Fail(L"ID3D12GraphicsCommandList::Reset", hr);
        return false;
    }

    const auto before = Transition(
        impl_->renderTargets[impl_->frameIndex].Get(),
        D3D12_RESOURCE_STATE_PRESENT,
        D3D12_RESOURCE_STATE_RENDER_TARGET);
    impl_->commandList->ResourceBarrier(1, &before);

    D3D12_CPU_DESCRIPTOR_HANDLE rtv =
        impl_->rtvHeap->GetCPUDescriptorHandleForHeapStart();
    rtv.ptr += impl_->frameIndex * impl_->rtvDescriptorSize;
    const float clearColor[] = {0.035f, 0.06f, 0.12f, 1.0f};
    impl_->commandList->OMSetRenderTargets(1, &rtv, FALSE, nullptr);
    impl_->commandList->ClearRenderTargetView(rtv, clearColor, 0, nullptr);

    if (scene != DemoScene::Clear) {
        D3D12_VIEWPORT viewport{0.0f, 0.0f, static_cast<float>(impl_->width),
                                static_cast<float>(impl_->height), 0.0f, 1.0f};
        D3D12_RECT scissor{0, 0, static_cast<LONG>(impl_->width),
                           static_cast<LONG>(impl_->height)};
        impl_->commandList->RSSetViewports(1, &viewport);
        impl_->commandList->RSSetScissorRects(1, &scissor);
        impl_->commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

        if (scene == DemoScene::TexturedQuad) {
            ID3D12DescriptorHeap* heaps[] = {impl_->srvHeap.Get()};
            impl_->commandList->SetDescriptorHeaps(_countof(heaps), heaps);
            impl_->commandList->SetGraphicsRootDescriptorTable(
                0, impl_->srvHeap->GetGPUDescriptorHandleForHeapStart());
            impl_->commandList->IASetVertexBuffers(0, 1, &impl_->quadView);
            impl_->commandList->DrawInstanced(6, 1, 0, 0);
        } else {
            impl_->commandList->IASetVertexBuffers(0, 1, &impl_->triangleView);
            impl_->commandList->DrawInstanced(3, 1, 0, 0);
        }
    }

    const auto after = Transition(
        impl_->renderTargets[impl_->frameIndex].Get(),
        D3D12_RESOURCE_STATE_RENDER_TARGET,
        D3D12_RESOURCE_STATE_PRESENT);
    impl_->commandList->ResourceBarrier(1, &after);
    hr = impl_->commandList->Close();
    if (FAILED(hr)) {
        impl_->Fail(L"ID3D12GraphicsCommandList::Close", hr);
        return false;
    }
    ID3D12CommandList* lists[] = {impl_->commandList.Get()};
    impl_->commandQueue->ExecuteCommandLists(1, lists);
    hr = impl_->swapChain->Present(1, 0);
    if (FAILED(hr)) {
        impl_->Fail(L"IDXGISwapChain3::Present", hr);
        return false;
    }
    if (!impl_->WaitForGpu()) {
        return false;
    }
    return true;
}

bool D3D12Backend::IsInitialized() const {
    return impl_ && impl_->initialized;
}

const std::wstring& D3D12Backend::Status() const {
    return impl_->status;
}

} // namespace ChonkyStation4::Xbox::Graphics
