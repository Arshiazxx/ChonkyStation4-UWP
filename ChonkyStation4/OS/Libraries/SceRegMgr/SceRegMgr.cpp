#include "SceRegMgr.hpp"
#include <Logger.hpp>
#include <Loaders/Module.hpp>


namespace PS4::OS::Libs::SceRegMgr {

MAKE_LOG_FUNCTION(log, lib_sceRegMgr);

void init(Module& module) {
    module.addSymbolExport("mPYKD12UDQI", "sceRegMgrGetInt", "libSceRegMgr", "libSceRegMgr", (void*)&sceRegMgrGetInt);
    
    module.addSymbolStub("CTplLrrndUg", "sceRegMgrGetStr", "libSceRegMgr", "libSceRegMgr");
    module.addSymbolStub("NqxMleeTiLs", "sceRegMgrGetBin", "libSceRegMgr", "libSceRegMgr");
    module.addSymbolStub("sywg-RnhZMA", "sceRegMgrSrvGetRegion", "libSceRegMgr", "libSceRegMgr");
    module.addSymbolStub("rebo0q4yREE", "sceRegMgrIsInitOK", "libSceRegMgr", "libSceRegMgr");
}

s32 PS4_FUNC sceRegMgrGetInt(s32 key, s32* data) {
    log("sceRegMgrGetInt(key=0x%x, data=*%p)\n", key, data);

    switch (key) {
    case SCE_REGMGR_ENT_KEY_SYSTEM_initialize:          *data = 1;  break;
    case SCE_REGMGR_ENT_KEY_SYSTEM_language:            *data = 1;  break;
    case SCE_REGMGR_ENT_KEY_SYSTEM_button_assign:       *data = 1;  break;
    case SCE_REGMGR_ENT_KEY_VIDEOOUT_reset_reso_flag:   *data = 1;  break;
    default:                                            *data = 0;  break;
    }

    return SCE_OK;
}

}   // End namespace PS4::OS::Libs::SceRegMgr