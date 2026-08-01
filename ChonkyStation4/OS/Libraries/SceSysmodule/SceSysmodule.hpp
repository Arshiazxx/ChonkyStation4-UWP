#pragma once

#include <Common.hpp>


class Module;

namespace PS4::OS::Libs::SceSysmodule {

void init(Module& module);

s32 PS4_FUNC sceSysmoduleLoadModuleByNameInternal(char* name);

}   // End namespace PS4::OS::Libs::SceSysmodule