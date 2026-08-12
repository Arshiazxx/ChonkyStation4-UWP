#pragma once

#include <Common.hpp>
#include <OS/Np/NpTypes.hpp>


class Module;

namespace PS4::OS::Libs::SceAppContent {

void init(Module& module);

static constexpr s32 SCE_APP_CONTENT_MOUNTPOINT_DATA_MAXSIZE = 16;

static constexpr s32 SCE_APP_CONTENT_APPPARAM_ID_SKU_FLAG               = 0;
static constexpr s32 SCE_APP_CONTENT_APPPARAM_ID_USER_DEFINED_PARAM_1   = 1;
static constexpr s32 SCE_APP_CONTENT_APPPARAM_ID_USER_DEFINED_PARAM_2   = 2;
static constexpr s32 SCE_APP_CONTENT_APPPARAM_ID_USER_DEFINED_PARAM_3   = 3;
static constexpr s32 SCE_APP_CONTENT_APPPARAM_ID_USER_DEFINED_PARAM_4   = 4;

using SceAppContentTemporaryDataOption = u32;
using SceAppContentAppParamId = u32;

struct SceAppContentMountPoint {
    char data[SCE_APP_CONTENT_MOUNTPOINT_DATA_MAXSIZE];
};

struct SceAppContentAddcontInfo;

s32 PS4_FUNC sceAppContentAppParamGetInt(SceAppContentAppParamId param_id, s32* value);
s32 PS4_FUNC sceAppContentTemporaryDataMount(SceAppContentTemporaryDataOption option, SceAppContentMountPoint* mount_point);
s32 PS4_FUNC sceAppContentTemporaryDataMount2(SceAppContentTemporaryDataOption option, SceAppContentMountPoint* mount_point);
s32 PS4_FUNC sceAppContentTemporaryDataGetAvailableSpaceKb(SceAppContentMountPoint* mount_point, size_t* available_space_kb);
s32 PS4_FUNC sceAppContentGetAddcontInfoList(Np::SceNpServiceLabel service_label, SceAppContentAddcontInfo* list, u32 n_list, u32* n_hit);

}   // End namespace PS4::OS::Libs::SceAppContent