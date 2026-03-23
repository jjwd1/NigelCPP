#pragma once
#include <windows.h>
#include <atomic>
#include <mutex>
#include <string>
#include "NigelConfig.hpp"
#include "../RLSDK/SDK_HEADERS/TAGame_classes.hpp"

extern std::string g_ToggleKey;
extern std::atomic<bool> g_BotEnabled;
extern std::atomic<bool> g_ModelOk;
extern FVehicleInputs g_LatestInput;
extern std::mutex g_InputMx;
extern NigelConfig g_NigelConfig;

namespace NigelBot {
std::wstring ExeDir();
std::wstring BootstrapPath();
bool LoadBootstrapConfig(NigelConfig& cfg);
void SetupDllSearch();
void ApplyConfig(const NigelConfig& cfg);
void Inference_Init();
void Inference_OnTick(float dt);
void OnAttach(HMODULE mod);
void OnDetach();
}
