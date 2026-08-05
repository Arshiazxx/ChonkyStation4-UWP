#pragma once

#include <Common.hpp>


class Module;

namespace PS4::OS::Libs::SceRegMgr {

void init(Module& module);

static constexpr s32 SCE_REGMGR_ENT_KEY_SYSTEM_initialize = 0x2040000;

s32 PS4_FUNC sceRegMgrGetInt(s32 key, s32* data);

}   // End namespace PS4::OS::Libs::SceRegMgr