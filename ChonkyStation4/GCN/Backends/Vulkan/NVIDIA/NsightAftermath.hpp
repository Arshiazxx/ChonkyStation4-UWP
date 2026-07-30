#pragma once

#include <Common.hpp>


namespace PS4::GCN::Vulkan::NVIDIA {

void initAftermath();
bool isAftermathEnabled();
void waitForCrashDump();
void endAftermath();

}   // End namespace PS4::GCN::Vulkan::NVIDIA