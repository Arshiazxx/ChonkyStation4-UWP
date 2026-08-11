#include "SceMbus.hpp"
#include <Logger.hpp>
#include <Loaders/Module.hpp>
#include <deque>
#include <mutex>
#include <semaphore>


namespace PS4::OS::Libs::SceMbus {

MAKE_LOG_FUNCTION(log, lib_sceMbus);

void init(Module& module) {
    module.addSymbolExport("puHrnP8V-dY", "sceMbusEventReceive", "libSceMbus", "libSceMbus", (void*)&sceMbusEventReceive);
    module.addSymbolExport("KRL-S9qBqXw", "sceMbusGetDeviceInfoByCondition_", "libSceMbus", "libSceMbus", (void*)&sceMbusGetDeviceInfoByCondition_);
    
    module.addSymbolStub("wRPXMGtkOq0", "sceMbusInit", "libSceMbus", "libSceMbus");
    module.addSymbolStub("c08SEHicDNU", "sceMbusEventCreate_", "libSceMbus", "libSceMbus");
    module.addSymbolStub("HgPSJ1kcnHM", "sceMbusEventCallbackFuncsInit_", "libSceMbus", "libSceMbus");
    module.addSymbolStub("0LkfqnKtPQg", "sceMbusEventCreate", "libSceMbus", "libSceMbus");
    module.addSymbolStub("edYHYROxzx4", "libSceMbus_edYHYROxzx4", "libSceMbus", "libSceMbus");
    module.addSymbolStub("wpm6Yq7c4YE", "sceMbusSetAutoLoginMode", "libSceMbus", "libSceMbus");
    module.addSymbolStub("Sq1DqijPveA", "sceMbusSetScratchDataUpdatedEventMask", "libSceMbus", "libSceMbus");

    auto login_thr = std::thread([&]() {
        std::this_thread::sleep_for(std::chrono::seconds(20));
        submitEvent({ .system = 1, .event_id = 1, .device_id = 100 });
    });
    login_thr.detach();
}

std::mutex queue_mtx;
std::counting_semaphore<256> queue_sem { 0 };
std::deque<SceMbusEvent> events;
void submitEvent(const SceMbusEvent& event) {
    const std::unique_lock<std::mutex> lk(queue_mtx);
    events.push_back(event);
    queue_sem.release();
}

SceMbusEvent readEvent() {
    queue_sem.acquire();

    SceMbusEvent event;
    {
        const std::unique_lock<std::mutex> lk(queue_mtx);
        event = events.front();
        events.pop_front();
    }
    return event;
}

s32 PS4_FUNC sceMbusEventReceive(u32 handle, SceMbusEvent* out_events, u32 n_events, u32 unk1 /* maybe timeout? */) {
    log("sceMbusEventReceive(handle=%d, out_events=*%p, n_events=%d, unk1=%d)\n", handle, out_events, n_events, unk1);

    if (n_events > 1)
        printf("sceMbusEventReceive: n_events > 1");
    
    out_events[0] = readEvent();
    return 1;
}

struct SceMbusDeviceInfo {
    u64 qword[32];
};

s32 PS4_FUNC sceMbusGetDeviceInfoByCondition_(u32* unk1, u64 unk2, void* unk3, u64 unk4, u64* out_n_devices, SceMbusDeviceInfo* out_info, u64 unk7) {
    log("sceMbusGetDeviceInfoByCondition_(unk1=%p, unk2=%lld, unk3=%p, unk4=%lld, out_n_devices=*%p, out_info=%p, unk7=%lld)\n", unk1, unk2, unk3, unk4, out_n_devices, out_info, unk7);
    
    // *unk1: uid?
    // unk2 don't know
    // unk3 don't know
    // unk4 type?
    // unk6 don't know
    // unk7 don't know
    
    log("uid: %d\n", *unk1);
    log("*unk3: *%d\n", *(u32*)unk3);

    //out_info[0].device_unique_id    = 1;
    out_info[0].qword[0] = 0x1;         
    *(u32*)((u8*)&out_info[0] + 0x0C) = 2;          
    *(u32*)((u8*)&out_info[0] + 0xC8) = 0;

    const auto n_devices = 1;
    *out_n_devices = 1;
    return 1;
}

}   // End namespace PS4::OS::Libs::SceMbus