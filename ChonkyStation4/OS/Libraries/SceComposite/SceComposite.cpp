#include "SceComposite.hpp"
#include <Logger.hpp>
#include <Configuration.hpp>
#include <Loaders/Module.hpp>
#include <OS/Libraries/Kernel/Kernel.hpp>
#include <OS/Libraries/SceVideoOut/SceVideoOut.hpp>
#include <OS/Libraries/SceGnmDriver/SceGnmDriver.hpp>
#include <GCN/GCN.hpp>


namespace PS4::OS::Libs::SceComposite {

MAKE_LOG_FUNCTION(log, lib_sceComposite);

void* sce_compositor_system_address = nullptr;
size_t sce_compositor_system_size = 0;
void* sce_compositor_video_address = nullptr;
size_t sce_compositor_video_size = 0;

void init(Module& module) {
    if (Configuration::is_vsh) {
        sce_compositor_system_address = nullptr;
        sce_compositor_video_address = nullptr;
        Libs::Kernel::sceKernelMapFlexibleMemory(&sce_compositor_system_address, 512_MB, 0, 0);
        sce_compositor_system_size = 512_MB;
        Libs::Kernel::sceKernelMapFlexibleMemory(&sce_compositor_video_address, 1_GB, 0, 0);
        sce_compositor_video_size = 1_GB;
    }

    module.addSymbolStub("IUlpGnuoR1c", "sceCompositorInitWithProcessOrder", "libSceComposite", "libSceComposite");
    module.addSymbolExport("T6CVkdCDO7o", "sceCompositorGetSystemAddress", "libSceComposite", "libSceComposite", (void*)&sceCompositorGetSystemAddress);
    module.addSymbolStub("N6ID0KNnzY8", "sceCompositorGetSystemSize", "libSceComposite", "libSceComposite", sce_compositor_system_size);
    module.addSymbolExport("bxt+muwit0w", "sceCompositorGetVideoAddress", "libSceComposite", "libSceComposite", (void*)&sceCompositorGetVideoAddress);
    module.addSymbolStub("FTQCTDU0b4g", "sceCompositorGetSystemSize", "libSceComposite", "libSceComposite", sce_compositor_video_size);
    module.addSymbolStub("G4Q8KNkb5XE", "sceCompositorAllocateIndex", "libSceComposite", "libSceComposite", 1);
    module.addSymbolStub("GgOrwi+9vcA", "sceCompositorLockCommandBuffer", "libSceComposite", "libSceComposite");
    module.addSymbolStub("1OXbuWLRxqI", "sceCompositorReleaseCommandBuffer", "libSceComposite", "libSceComposite");
    module.addSymbolStub("eLU8pDi9KN0", "sceCompositorSetResolutionCommand", "libSceComposite", "libSceComposite");
    module.addSymbolStub("KpuS3CIcckw", "libSceComposite_KpuS3CIcckw", "libSceComposite", "libSceComposite");
    module.addSymbolStub("XZzyQX4T5ts", "libSceComposite_XZzyQX4T5ts", "libSceComposite", "libSceComposite");
    module.addSymbolStub("q+Qw1ESxCj8", "sceCompositorIsDebugCaptureEnabled", "libSceComposite", "libSceComposite");
    module.addSymbolStub("YzI2BOoDw+I", "sceCompositorSetPatchCommand", "libSceComposite", "libSceComposite");
    module.addSymbolExport("DhtKelVAIaA", "sceCompositorSetGnmContextCommand", "libSceComposite", "libSceComposite", (void*)&sceCompositorSetGnmContextCommand);
    module.addSymbolExport("1oTrw-ivVpA", "sceCompositorSetFlipCommand", "libSceComposite", "libSceComposite", (void*)&sceCompositorSetFlipCommand);
    module.addSymbolExport("3Q85e5cS3e0", "sceCompositorSetPostEventCommand", "libSceComposite", "libSceComposite", (void*)&sceCompositorSetPostEventCommand);
    module.addSymbolExport("twGXom56jw0", "sceCompositorGetRenderTargetResolution", "libSceComposite", "libSceComposite", (void*)&sceCompositorGetRenderTargetResolution);
    module.addSymbolStub("qZNF03+ghLI", "sceCompositorFlush", "libSceComposite", "libSceComposite");
    module.addSymbolExport("4yWqjTZtvs4", "sceCompsoitorGetGpuClock", "libSceComposite", "libSceComposite", (void*)&sceCompsoitorGetGpuClock);
    module.addSymbolStub("6bz4VVSSFyg", "sceCompositorCommandGpuPerfBegin", "libSceComposite", "libSceComposite");
    module.addSymbolStub("fH2IStnGK4M", "sceCompositorCommandGpuPerfEnd", "libSceComposite", "libSceComposite");
    module.addSymbolStub("H4EXZ9L3p2M", "sceCompositorSetMorpheusState", "libSceComposite", "libSceComposite");
    module.addSymbolExport("deKovf3qViA", "sceCompositorWaitPostEvent", "libSceComposite", "libSceComposite", (void*)&sceCompositorWaitPostEvent);
}

void* PS4_FUNC sceCompositorGetSystemAddress() {
    log("sceCompositorGetSystemAddress()\n");
    return sce_compositor_system_address;
}

void* PS4_FUNC sceCompositorGetVideoAddress() {
    log("sceCompositorGetVideoAddress()\n");
    return sce_compositor_video_address;
}

s32 PS4_FUNC sceCompositorGetRenderTargetResolution(s16* width, s16* height) {
    log("sceCompositorGetRenderTargetResolution(width=*%p, height=*%p)\n", width, height);

    *width = 1920;
    *height = 1080;
    return SCE_OK;
}

s32 PS4_FUNC sceCompsoitorGetGpuClock(u64* gpu_clock) {
    log("sceCompsoitorGetGpuClock(gpu_clock=*%p)\n", gpu_clock);

    *gpu_clock = std::chrono::system_clock::now().time_since_epoch().count();
    return SCE_OK;
}

s32 PS4_FUNC sceCompositorWaitPostEvent() {
    log("sceCompositorWaitPostEvent()\n");

    while (!PS4::GCN::isCommandProcessorIdle()) std::this_thread::sleep_for(std::chrono::microseconds(100));
    return SCE_OK;
}

u32* compositor_dcb_gpu_addr = nullptr;
u32 compositor_dcb_size = 0;
s32 PS4_FUNC sceCompositorSetGnmContextCommand(u32* dcb_gpu_addr, u32 dcb_size, u32* ccb_gpu_addr, u32 ccb_size) {
    log("sceCompositorSetGnmContextCommand(dcb_gpu_addr=*%p, dcb_size=%d, ccb_gpu_addr=*%p, ccb_size=%d)\n", dcb_gpu_addr, dcb_size, ccb_gpu_addr, ccb_size);
    compositor_dcb_gpu_addr = dcb_gpu_addr;
    compositor_dcb_size = dcb_size;
    return PS4::OS::Libs::SceGnmDriver::sceGnmSubmitCommandBuffers(1, &compositor_dcb_gpu_addr, &compositor_dcb_size, nullptr, nullptr);
}

s32 PS4_FUNC sceCompositorSetFlipCommand() {
    log("sceCompositorSetFlipCommand()\n");
    PS4::OS::Libs::SceVideoOut::sceVideoOutSubmitFlip(2, 0, 0, 0);
    return SCE_OK;
}

s32 PS4_FUNC sceCompositorSetPostEventCommand(void* cmd) {
    log("sceCompositorSetPostEventCommand(cmd=%p)\n", cmd);

    return SCE_OK;
}

}   // End namespace PS4::OS::Libs::SceComposite