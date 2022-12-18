#include "main.hpp"

#include <unordered_map>
#include <filesystem>
#include <vector>

#include "ARCropolisAPI.hpp"
#include "resource.hpp"


int main() {
    skyline::TcpLogger::SendRaw("[arcaveman] entered main.\n");
	InitEx();
	// A64InlineHook((uint8_t*)skyline::utils::getRegionAddress(skyline::utils::region::Text) + 0x37A1720, &test_hook1);
	// A64InlineHook((uint8_t*)skyline::utils::getRegionAddress(skyline::utils::region::Text) + 0x37A1470, &che);
	// ARCropolisAPI::RegisterEventCallback(ARCropolisAPI::Event::ARCFilesystemMounted, &arcaveman_init);
	// ARCropolisAPI::RegisterCallback(Hash40("ui/replace_patch/chara/chara_1/chara_1_pickel_00.bntx"), 300000, &steve_callback);
}