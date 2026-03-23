#include "pch.hpp"
#include "DLL/IPC.hpp"
#include "Bot/Bot.hpp"
#include <thread>

NigelPipeClient g_NigelLog;

static void InitUI() {}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID) {
	switch (ul_reason_for_call) {
	case DLL_PROCESS_ATTACH:
		DisableThreadLibraryCalls(hModule);
		g_NigelLog.Connect();
		std::thread([hModule]{
			InitUI();
			NigelBot::OnAttach(hModule);
		}).detach();
		break;
	case DLL_PROCESS_DETACH:
		NigelBot::OnDetach();
		g_NigelLog.Disconnect();
		break;
	default:
		break;
	}
	return TRUE;
}
