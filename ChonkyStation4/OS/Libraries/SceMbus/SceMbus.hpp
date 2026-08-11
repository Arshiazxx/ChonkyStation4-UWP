#pragma once

#include <Common.hpp>


class Module;

namespace PS4::OS::Libs::SceMbus {

void init(Module& module);

struct SceMbusEvent {
    u32 system;
    u32 event_id;
    u64 device_id;
    u32 unk1;
    u32 subsystem;
    u64 unk2;
    u64 unk3;
    u64 unk4;
};

void submitEvent(const SceMbusEvent& event);

struct SceMbusDeviceInfo;

s32 PS4_FUNC sceMbusEventReceive(u32 handle, SceMbusEvent* out_events, u32 n_events, u32 unk1 /* maybe timeout? */);
s32 PS4_FUNC sceMbusGetDeviceInfoByCondition_(u32* unk1, u64 unk2, void* unk3, u64 unk4, u64* out_n_devices, SceMbusDeviceInfo* out_info, u64 unk7);

}   // End namespace PS4::OS::Libs::SceMbus