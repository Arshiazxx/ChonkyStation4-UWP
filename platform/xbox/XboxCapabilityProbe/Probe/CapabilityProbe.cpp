#include "pch.h"
#include "CapabilityProbe.hpp"

#include <d3d12.h>
#include <dxgi1_6.h>
#include <windows.gaming.input.h>
#include <windows.media.devices.h>
#include <windows.networking.connectivity.h>
#include <windows.storage.h>
#include <windows.system.profile.h>

using Microsoft::WRL::ComPtr;
using namespace Windows::ApplicationModel;
using namespace Windows::Gaming::Input;
using namespace Windows::Media::Devices;
using namespace Windows::Networking::Connectivity;
using namespace Windows::Storage;
using namespace Windows::System;
using namespace Windows::System::Profile;

namespace XboxCapabilityProbe {
namespace Probe {

namespace {

constexpr const char* kSourceCommit = "310269290a3c256f5911d4bc7e441489bffffbf6";
constexpr const char* kSupported = "SUPPORTED";
constexpr const char* kUnsupported = "UNSUPPORTED";
constexpr const char* kUnknown = "UNKNOWN";
constexpr const char* kRequiresHardware = "REQUIRES_HARDWARE_TEST";

std::string ToUtf8(Platform::String^ value) {
    if (value == nullptr || value->IsEmpty()) return {};
    const wchar_t* wide = value->Data();
    const int required = WideCharToMultiByte(CP_UTF8, 0, wide, -1, nullptr, 0, nullptr, nullptr);
    if (required <= 1) return {};
    std::string result(static_cast<size_t>(required), '\0');
    const int written = WideCharToMultiByte(CP_UTF8, 0, wide, -1, &result[0], required, nullptr, nullptr);
    if (written <= 0) return {};
    if (!result.empty() && result.back() == '\0') result.pop_back();
    return result;
}

std::string EscapeJson(const std::string& value) {
    std::ostringstream out;
    for (unsigned char c : value) {
        switch (c) {
        case '\\': out << "\\\\"; break;
        case '"': out << "\\\""; break;
        case '\b': out << "\\b"; break;
        case '\f': out << "\\f"; break;
        case '\n': out << "\\n"; break;
        case '\r': out << "\\r"; break;
        case '\t': out << "\\t"; break;
        default:
            if (c < 0x20) {
                const char hex[] = "0123456789abcdef";
                out << "\\u00" << hex[(c >> 4) & 0xf] << hex[c & 0xf];
            } else {
                out << static_cast<char>(c);
            }
        }
    }
    return out.str();
}

std::string XboxStatus(bool isXbox, bool success) {
    if (!isXbox) return kRequiresHardware;
    return success ? kSupported : kUnsupported;
}

void Add(ProbeReport& report, const std::string& category, const std::string& name,
         bool isXbox, bool success, const std::string& detail) {
    report.entries.push_back({category, name, XboxStatus(isXbox, success), success ? "PASS" : "FAIL", detail});
}

void AddNotTested(ProbeReport& report, const std::string& category, const std::string& name,
                  const std::string& status, const std::string& detail) {
    report.entries.push_back({category, name, status, "NOT_TESTED", detail});
}

std::string BytesText(std::uint64_t bytes) {
    std::ostringstream out;
    out << bytes << " bytes (" << (bytes / (1024ull * 1024ull)) << " MiB)";
    return out.str();
}

std::string FeatureLevelText(D3D_FEATURE_LEVEL level) {
    switch (level) {
    case D3D_FEATURE_LEVEL_12_1: return "12_1";
    case D3D_FEATURE_LEVEL_12_0: return "12_0";
    case D3D_FEATURE_LEVEL_11_1: return "11_1";
    case D3D_FEATURE_LEVEL_11_0: return "11_0";
    default: return "unknown";
    }
}

std::string ShaderModelText(D3D_SHADER_MODEL model) {
    const unsigned value = static_cast<unsigned>(model);
    std::ostringstream out;
    out << ((value >> 4) & 0xf) << '.' << (value & 0xf);
    return out.str();
}

void ProbeMemory(ProbeReport& report, bool isXbox) {
    SYSTEM_INFO info{};
    GetSystemInfo(&info);
    Add(report, "memory", "system_page_size", isXbox, info.dwPageSize != 0,
        "page_size=" + std::to_string(info.dwPageSize));
    Add(report, "memory", "allocation_granularity", isXbox, info.dwAllocationGranularity != 0,
        "allocation_granularity=" + std::to_string(info.dwAllocationGranularity));

    const auto usage = MemoryManager::AppMemoryUsage;
    const auto limit = MemoryManager::AppMemoryUsageLimit;
    Add(report, "memory", "app_memory_usage_limit", isXbox, limit > 0,
        "usage=" + BytesText(usage) + "; limit=" + BytesText(limit));

    constexpr std::array<std::uint64_t, 5> reserveSizes = {
        256ull * 1024 * 1024,
        512ull * 1024 * 1024,
        1ull * 1024 * 1024 * 1024,
        2ull * 1024 * 1024 * 1024,
        4ull * 1024 * 1024 * 1024,
    };
    std::uint64_t largestReserve = 0;
    for (auto size : reserveSizes) {
        void* p = VirtualAllocFromApp(nullptr, static_cast<SIZE_T>(size), MEM_RESERVE, PAGE_NOACCESS);
        if (p) {
            largestReserve = size;
            VirtualFree(p, 0, MEM_RELEASE);
        } else {
            break;
        }
    }
    Add(report, "memory", "large_address_reservation", isXbox, largestReserve > 0,
        "largest bounded reservation attempted successfully=" + BytesText(largestReserve));

    constexpr SIZE_T commitSize = 16ull * 1024 * 1024;
    auto* committed = static_cast<std::uint8_t*>(VirtualAllocFromApp(nullptr, commitSize, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
    bool commitOk = committed != nullptr;
    if (commitOk) {
        committed[0] = 0x5a;
        committed[commitSize - 1] = 0xa5;
        commitOk = committed[0] == 0x5a && committed[commitSize - 1] == 0xa5;
        VirtualFree(committed, 0, MEM_RELEASE);
    }
    Add(report, "memory", "reserve_and_commit_16mib", isXbox, commitOk, "bounded commit/write/read test");

    const SIZE_T exactSize = std::max<SIZE_T>(info.dwAllocationGranularity, 64 * 1024);
    void* first = VirtualAllocFromApp(nullptr, exactSize, MEM_RESERVE, PAGE_NOACCESS);
    bool exactOk = false;
    if (first) {
        void* desired = first;
        VirtualFree(first, 0, MEM_RELEASE);
        void* second = VirtualAllocFromApp(desired, exactSize, MEM_RESERVE, PAGE_NOACCESS);
        exactOk = second == desired;
        if (second) VirtualFree(second, 0, MEM_RELEASE);
    }
    Add(report, "virtual_memory", "fixed_address_reservation", isXbox, exactOk,
        "reserve/release/re-reserve same address; no arbitrary guest address used");

    const SIZE_T pageSize = std::max<SIZE_T>(info.dwPageSize, 4096);
    auto* page = static_cast<std::uint8_t*>(VirtualAllocFromApp(nullptr, pageSize, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
    bool readOnlyOk = false;
    bool rxOk = false;
    bool rwRestoreOk = false;
    bool flushOk = false;
    if (page) {
        page[0] = 0xc3; // data byte only; M5 does not execute it.
        DWORD oldProtect = 0;
        readOnlyOk = VirtualProtectFromApp(page, pageSize, PAGE_READONLY, &oldProtect) != 0;
        if (readOnlyOk) {
            DWORD ignored = 0;
            rxOk = VirtualProtectFromApp(page, pageSize, PAGE_EXECUTE_READ, &ignored) != 0;
            flushOk = FlushInstructionCache(GetCurrentProcess(), page, pageSize) != 0;
            DWORD ignored2 = 0;
            rwRestoreOk = VirtualProtectFromApp(page, pageSize, PAGE_READWRITE, &ignored2) != 0;
        }
        VirtualFree(page, 0, MEM_RELEASE);
    }
    Add(report, "virtual_memory", "page_protection_read_only", isXbox, readOnlyOk, "VirtualProtect PAGE_READONLY");
    Add(report, "virtual_memory", "page_protection_execute_read", isXbox, rxOk, "VirtualProtect PAGE_EXECUTE_READ; generated bytes not executed");
    Add(report, "jit_related", "instruction_cache_flush_api", isXbox, flushOk, "FlushInstructionCache after protection change");
    Add(report, "virtual_memory", "page_protection_restore_read_write", isXbox, rwRestoreOk, "VirtualProtect back to PAGE_READWRITE");

    auto* executableCandidate = static_cast<std::uint8_t*>(VirtualAllocFromApp(nullptr, pageSize, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
    bool executableTransitionOk = false;
    if (executableCandidate) {
        executableCandidate[0] = 0xc3; // data byte only; M5 never calls it.
        DWORD oldProtection = 0;
        executableTransitionOk = VirtualProtectFromApp(executableCandidate, pageSize, PAGE_EXECUTE_READ, &oldProtection) != 0;
        if (executableTransitionOk) FlushInstructionCache(GetCurrentProcess(), executableCandidate, pageSize);
        VirtualFree(executableCandidate, 0, MEM_RELEASE);
    }
    Add(report, "jit_related", "executable_memory_allocation", isXbox, executableTransitionOk,
        "UWP W->RX test using VirtualAllocFromApp + VirtualProtectFromApp; no RWX page and no generated-code call");

    AddNotTested(report, "jit_related", "generated_code_execution", kRequiresHardware,
        "Intentionally not executed in M5; native guest/JIT execution is the M7 gate");
    AddNotTested(report, "jit_related", "guest_tls_abi", kRequiresHardware,
        "Not probed in M5; guest TLS/ABI execution belongs to M7");
}

void ProbeThreading(ProbeReport& report, bool isXbox) {
    const unsigned hw = std::thread::hardware_concurrency();
    Add(report, "cpu", "hardware_concurrency_query", isXbox, hw > 0, "reported=" + std::to_string(hw));

    constexpr unsigned kThreadCount = 32;
    std::atomic<unsigned> completed{0};
    bool threadOk = true;
    try {
        std::vector<std::thread> threads;
        threads.reserve(kThreadCount);
        for (unsigned i = 0; i < kThreadCount; ++i) {
            threads.emplace_back([&completed] { completed.fetch_add(1, std::memory_order_relaxed); });
        }
        for (auto& thread : threads) thread.join();
        threadOk = completed.load(std::memory_order_relaxed) == kThreadCount;
    } catch (...) {
        threadOk = false;
    }
    Add(report, "cpu", "thread_batch_32", isXbox, threadOk, "created/joined 32 std::thread workers");

    std::mutex mutex;
    std::condition_variable cv;
    bool ready = false;
    bool woke = false;
    std::thread waiter([&] {
        std::unique_lock<std::mutex> lock(mutex);
        cv.wait(lock, [&] { return ready; });
        woke = true;
    });
    {
        std::lock_guard<std::mutex> lock(mutex);
        ready = true;
    }
    cv.notify_one();
    waiter.join();
    Add(report, "cpu", "mutex_condition_variable", isXbox, woke, "std::mutex + std::condition_variable wake/join");

    bool monotonic = true;
    auto previous = std::chrono::steady_clock::now();
    for (int i = 0; i < 1000; ++i) {
        auto now = std::chrono::steady_clock::now();
        if (now < previous) monotonic = false;
        previous = now;
    }
    Add(report, "cpu", "steady_clock_monotonic_sample", isXbox, monotonic, "1000 sequential steady_clock samples");
}

void ProbeGraphics(ProbeReport& report, bool isXbox) {
    ComPtr<IDXGIFactory4> factory;
    HRESULT hr = CreateDXGIFactory2(0, IID_PPV_ARGS(&factory));
    if (FAILED(hr)) {
        Add(report, "graphics", "d3d12_device", isXbox, false, "CreateDXGIFactory2 failed");
        AddNotTested(report, "graphics", "feature_level", kUnknown, "No DXGI factory/device");
        AddNotTested(report, "graphics", "shader_model", kUnknown, "No D3D12 device");
        return;
    }

    ComPtr<IDXGIAdapter1> adapter;
    ComPtr<ID3D12Device> device;
    std::string adapterName;
    for (UINT index = 0; ; ++index) {
        ComPtr<IDXGIAdapter1> candidate;
        if (factory->EnumAdapters1(index, &candidate) == DXGI_ERROR_NOT_FOUND) break;
        DXGI_ADAPTER_DESC1 desc{};
        candidate->GetDesc1(&desc);
        if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) continue;
        if (SUCCEEDED(D3D12CreateDevice(candidate.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device)))) {
            adapter = candidate;
            adapterName = ToUtf8(ref new Platform::String(desc.Description));
            break;
        }
    }

    const bool deviceOk = device != nullptr;
    Add(report, "graphics", "d3d12_device", isXbox, deviceOk, deviceOk ? "hardware adapter=" + adapterName : "no hardware D3D12 adapter accepted FL11_0");
    if (!device) {
        AddNotTested(report, "graphics", "feature_level", kUnknown, "No D3D12 device");
        AddNotTested(report, "graphics", "shader_model", kUnknown, "No D3D12 device");
        return;
    }

    const D3D_FEATURE_LEVEL requestedLevels[] = {
        D3D_FEATURE_LEVEL_12_1, D3D_FEATURE_LEVEL_12_0, D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0
    };
    D3D12_FEATURE_DATA_FEATURE_LEVELS levels{};
    levels.NumFeatureLevels = _countof(requestedLevels);
    levels.pFeatureLevelsRequested = requestedLevels;
    hr = device->CheckFeatureSupport(D3D12_FEATURE_FEATURE_LEVELS, &levels, sizeof(levels));
    Add(report, "graphics", "feature_level", isXbox, SUCCEEDED(hr), SUCCEEDED(hr) ? FeatureLevelText(levels.MaxSupportedFeatureLevel) : "CheckFeatureSupport failed");

    D3D12_FEATURE_DATA_SHADER_MODEL shader{};
    shader.HighestShaderModel = D3D_SHADER_MODEL_6_0;
    hr = device->CheckFeatureSupport(D3D12_FEATURE_SHADER_MODEL, &shader, sizeof(shader));
    Add(report, "graphics", "shader_model", isXbox, SUCCEEDED(hr), SUCCEEDED(hr) ? ShaderModelText(shader.HighestShaderModel) : "CheckFeatureSupport failed");

    D3D12_FEATURE_DATA_ROOT_SIGNATURE rootSig{};
    rootSig.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;
    hr = device->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &rootSig, sizeof(rootSig));
    Add(report, "graphics", "root_signature_version", isXbox, SUCCEEDED(hr), SUCCEEDED(hr) ? (rootSig.HighestVersion == D3D_ROOT_SIGNATURE_VERSION_1_1 ? "1.1" : "1.0") : "CheckFeatureSupport failed");

    const DXGI_FORMAT formats[] = {
        DXGI_FORMAT_B8G8R8A8_UNORM,
        DXGI_FORMAT_R8G8B8A8_UNORM,
        DXGI_FORMAT_R32G32B32A32_FLOAT,
        DXGI_FORMAT_BC1_UNORM,
        DXGI_FORMAT_D32_FLOAT,
    };
    const char* formatNames[] = {"BGRA8_UNORM", "RGBA8_UNORM", "RGBA32_FLOAT", "BC1_UNORM", "D32_FLOAT"};
    for (size_t i = 0; i < _countof(formats); ++i) {
        D3D12_FEATURE_DATA_FORMAT_SUPPORT format{};
        format.Format = formats[i];
        hr = device->CheckFeatureSupport(D3D12_FEATURE_FORMAT_SUPPORT, &format, sizeof(format));
        const bool ok = SUCCEEDED(hr) && format.Support1 != D3D12_FORMAT_SUPPORT1_NONE;
        Add(report, "graphics_format", formatNames[i], isXbox, ok,
            SUCCEEDED(hr) ? "support1=" + std::to_string(static_cast<unsigned>(format.Support1)) + "; support2=" + std::to_string(static_cast<unsigned>(format.Support2)) : "CheckFeatureSupport failed");
    }

    const UINT descriptorCounts[] = {1024, 65536, 1000000};
    UINT largestDescriptorHeap = 0;
    for (UINT count : descriptorCounts) {
        D3D12_DESCRIPTOR_HEAP_DESC desc{};
        desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        desc.NumDescriptors = count;
        desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        ComPtr<ID3D12DescriptorHeap> heap;
        if (SUCCEEDED(device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&heap)))) largestDescriptorHeap = count;
        else break;
    }
    Add(report, "graphics", "shader_visible_descriptor_heap", isXbox, largestDescriptorHeap > 0,
        "largest bounded descriptor count created=" + std::to_string(largestDescriptorHeap));

    const UINT64 resourceSizes[] = {16ull * 1024 * 1024, 64ull * 1024 * 1024};
    UINT64 largestResource = 0;
    for (UINT64 size : resourceSizes) {
        D3D12_HEAP_PROPERTIES heap{};
        heap.Type = D3D12_HEAP_TYPE_DEFAULT;
        heap.CreationNodeMask = 1;
        heap.VisibleNodeMask = 1;
        D3D12_RESOURCE_DESC desc{};
        desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        desc.Width = size;
        desc.Height = 1;
        desc.DepthOrArraySize = 1;
        desc.MipLevels = 1;
        desc.SampleDesc.Count = 1;
        desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        ComPtr<ID3D12Resource> resource;
        if (SUCCEEDED(device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&resource)))) largestResource = size;
        else break;
    }
    Add(report, "graphics", "committed_buffer_resource", isXbox, largestResource > 0,
        "largest bounded buffer created=" + BytesText(largestResource));
}

void ProbeUwp(ProbeReport& report, bool isXbox) {
    const auto localFolder = ApplicationData::Current->LocalFolder;
    const std::wstring root(localFolder->Path->Data());
    const std::wstring testPath = root + L"\\capability-file-test.bin";
    bool fileOk = false;
    {
        std::ofstream out(testPath, std::ios::binary | std::ios::trunc);
        const char bytes[] = {'C', 'S', '4', 'X'};
        out.write(bytes, sizeof(bytes));
        out.close();
        std::ifstream in(testPath, std::ios::binary);
        char read[4]{};
        in.read(read, sizeof(read));
        fileOk = in.gcount() == 4 && std::memcmp(read, bytes, 4) == 0;
    }
    DeleteFileW(testPath.c_str());
    Add(report, "uwp", "local_filesystem_read_write", isXbox, fileOk, "ApplicationData LocalFolder create/write/read/delete");

    bool networkApiOk = false;
    std::string networkDetail;
    try {
        auto profile = NetworkInformation::GetInternetConnectionProfile();
        networkApiOk = true;
        networkDetail = profile ? "network profile available" : "API available; no active internet profile";
    } catch (Platform::Exception^ e) {
        networkDetail = "NetworkInformation exception: " + ToUtf8(e->Message);
    }
    Add(report, "uwp", "networking_api", isXbox, networkApiOk, networkDetail);
    AddNotTested(report, "uwp", "network_connectivity", kRequiresHardware,
        "No external endpoint is contacted by the probe; actual Xbox networking remains a runtime test");

    const auto gamepadCount = Gamepad::Gamepads->Size;
    Add(report, "uwp", "gamepad_api", isXbox, true, "Gamepad API accessible; currently visible gamepads=" + std::to_string(gamepadCount));
    if (gamepadCount == 0) {
        AddNotTested(report, "uwp", "physical_controller_input", kRequiresHardware, "No controller was present during this run");
    } else {
        Add(report, "uwp", "physical_controller_input", isXbox, true, "at least one Gamepad object present");
    }

    bool audioOk = false;
    std::string audioDetail;
    try {
        auto audioId = MediaDevice::GetDefaultAudioRenderId(Windows::Media::Devices::AudioDeviceRole::Default);
        audioOk = audioId != nullptr && !audioId->IsEmpty();
        audioDetail = audioOk ? "default audio render endpoint id available" : "default audio render endpoint id empty";
    } catch (Platform::Exception^ e) {
        audioDetail = "MediaDevice exception: " + ToUtf8(e->Message);
    }
    Add(report, "uwp", "audio_output_endpoint", isXbox, audioOk, audioDetail);

    Add(report, "uwp", "application_launch", isXbox, true, "Probe code is running inside the packaged application");
    AddNotTested(report, "uwp", "suspend_resume_lifecycle", kRequiresHardware,
        "Requires an actual suspend/resume cycle; launch alone does not validate lifecycle recovery");
}

} // namespace

std::string ProbeReport::ToJson() const {
    std::ostringstream out;
    out << "{\n";
    out << "  \"schema_version\": 1,\n";
    out << "  \"source_commit\": \"" << EscapeJson(sourceCommit) << "\",\n";
    out << "  \"run_state\": \"" << EscapeJson(runState) << "\",\n";
    out << "  \"device_family\": \"" << EscapeJson(deviceFamily) << "\",\n";
    out << "  \"results\": [\n";
    for (size_t i = 0; i < entries.size(); ++i) {
        const auto& entry = entries[i];
        out << "    {\"category\": \"" << EscapeJson(entry.category)
            << "\", \"name\": \"" << EscapeJson(entry.name)
            << "\", \"status\": \"" << EscapeJson(entry.status)
            << "\", \"observed\": \"" << EscapeJson(entry.observed)
            << "\", \"detail\": \"" << EscapeJson(entry.detail) << "\"}";
        if (i + 1 != entries.size()) out << ',';
        out << '\n';
    }
    out << "  ]\n}";
    return out.str();
}

std::wstring ProbeReport::ToSummary() const {
    std::wstringstream out;
    out << L"Device family: " << std::wstring(deviceFamily.begin(), deviceFamily.end()) << L"\n";
    out << L"Source commit: " << std::wstring(sourceCommit.begin(), sourceCommit.end()) << L"\n\n";
    for (const auto& entry : entries) {
        out << L"[" << std::wstring(entry.status.begin(), entry.status.end()) << L"] "
            << std::wstring(entry.category.begin(), entry.category.end()) << L"/"
            << std::wstring(entry.name.begin(), entry.name.end()) << L" â€” "
            << std::wstring(entry.observed.begin(), entry.observed.end()) << L" â€” "
            << std::wstring(entry.detail.begin(), entry.detail.end()) << L"\n";
    }
    return out.str();
}

ProbeReport CapabilityProbe::Run() {
    ProbeReport report;
    report.sourceCommit = kSourceCommit;
    report.runState = "COMPLETED";
    report.deviceFamily = ToUtf8(AnalyticsInfo::VersionInfo->DeviceFamily);
    const bool isXbox = report.deviceFamily == "Windows.Xbox";

    ProbeMemory(report, isXbox);
    ProbeThreading(report, isXbox);
    ProbeGraphics(report, isXbox);
    ProbeUwp(report, isXbox);
    return report;
}

Platform::String^ CapabilityProbe::WriteJson(const ProbeReport& report) {
    const std::wstring root(ApplicationData::Current->LocalFolder->Path->Data());
    const std::wstring path = root + L"\\xbox-capabilities.json";
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    const auto json = report.ToJson();
    out.write(json.data(), static_cast<std::streamsize>(json.size()));
    out.close();
    if (!out) throw ref new Platform::FailureException(L"Failed to write xbox-capabilities.json");
    return ref new Platform::String(path.c_str());
}

} // namespace Probe
} // namespace XboxCapabilityProbe




