#include "NigelUI.hpp"
#include "../../ImGui/imgui.h"
#include "../../OutWrapper.h"
#include "../../Components/Components/GUI.hpp"
#include "../../Bot/Bot.hpp"
#include <windows.h>
#include <atomic>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <filesystem>
#include <fstream>
#include <sstream>

static std::atomic<bool> g_nigelUnloading{false};
static DWORD WINAPI nigel_unload_thr(void*){
    g_NigelLog.Log("nigel sdk unloading");
    g_NigelLog.Log("__NIGEL_EXIT__");
    Sleep(100);
    GUIComponent::Unload();
    HMODULE m=nullptr;
    if(!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,(LPCSTR)&nigel_unload_thr,&m)) return 0;
    FreeLibraryAndExitThread(m,0);
    return 0;
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
static float c255(int v){ return (float)v/255.0f; }
static ImVec4 rgba(int r,int g,int b,float a){ return ImVec4(c255(r),c255(g),c255(b),a); }
static bool is09(char c){ return c>='0'&&c<='9'; }
static void parse_ints(const char* s, std::vector<int>& out) {
    out.clear();
    if (!s) return;
    const char* p = s;
    while (*p) {
        if (is09(*p) || (*p == '-' && is09(p[1]))) {
            out.push_back(std::atoi(p));
            if (*p == '-') p++;
            while (*p && is09(*p)) p++;
        } else {
            p++;
        }
    }
    if (out.empty()) out.push_back(1024);
}

// ---------------------------------------------------------------------------
// Config persistence
// ---------------------------------------------------------------------------
static std::filesystem::path GetConfigFilePath(){
    wchar_t dllPath[MAX_PATH]{0};
    HMODULE hm=nullptr;
    GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,(LPCSTR)&nigel_unload_thr,&hm);
    GetModuleFileNameW(hm, dllPath, MAX_PATH);
    return std::filesystem::path(dllPath).parent_path() / "nigel_config.cfg";
}

struct SavedConfig {
    char toggleKey[8]="R";
    char botType[64]="GGL_PPO";
    char policySize[32]="256, 256, 256";
    bool useSharedHead=true;
    char sharedHeadSize[32]="256, 256";
    char obsBuilder[64]="AdvancedObs";
    int obsSize=109;
    int tickSkip=8;
    int actionDelay=7;
    char device[16]="cpu";
    int activationIdx=0;
};

static bool SaveConfigFile(const SavedConfig& c){
    auto path=GetConfigFilePath();
    std::ofstream f(path);
    if(!f.is_open()) return false;
    f << "toggleKey=" << c.toggleKey << "\n";
    f << "botType=" << c.botType << "\n";
    f << "policySize=" << c.policySize << "\n";
    f << "useSharedHead=" << (c.useSharedHead?"1":"0") << "\n";
    f << "sharedHeadSize=" << c.sharedHeadSize << "\n";
    f << "obsBuilder=" << c.obsBuilder << "\n";
    f << "obsSize=" << c.obsSize << "\n";
    f << "tickSkip=" << c.tickSkip << "\n";
    f << "actionDelay=" << c.actionDelay << "\n";
    f << "device=" << c.device << "\n";
    f << "activationIdx=" << c.activationIdx << "\n";
    f.close();
    return true;
}

static bool LoadConfigFile(SavedConfig& c){
    auto path=GetConfigFilePath();
    std::ifstream f(path);
    if(!f.is_open()) return false;
    std::string line;
    while(std::getline(f,line)){
        auto eq=line.find('=');
        if(eq==std::string::npos) continue;
        std::string key=line.substr(0,eq);
        std::string val=line.substr(eq+1);
        if(key=="toggleKey") strncpy_s(c.toggleKey,val.c_str(),sizeof(c.toggleKey)-1);
        else if(key=="botType") strncpy_s(c.botType,val.c_str(),sizeof(c.botType)-1);
        else if(key=="policySize") strncpy_s(c.policySize,val.c_str(),sizeof(c.policySize)-1);
        else if(key=="useSharedHead") c.useSharedHead=(val=="1");
        else if(key=="sharedHeadSize") strncpy_s(c.sharedHeadSize,val.c_str(),sizeof(c.sharedHeadSize)-1);
        else if(key=="obsBuilder") strncpy_s(c.obsBuilder,val.c_str(),sizeof(c.obsBuilder)-1);
        else if(key=="obsSize") try{ c.obsSize=std::stoi(val); }catch(...){}
        else if(key=="tickSkip") try{ c.tickSkip=std::stoi(val); }catch(...){}
        else if(key=="actionDelay") try{ c.actionDelay=std::stoi(val); }catch(...){}
        else if(key=="device") strncpy_s(c.device,val.c_str(),sizeof(c.device)-1);
        else if(key=="activationIdx") try{ c.activationIdx=std::stoi(val); if(c.activationIdx<0||c.activationIdx>7) c.activationIdx=0; }catch(...){}
    }
    return true;
}

// ---------------------------------------------------------------------------
// Theme
// ---------------------------------------------------------------------------
static const int kNumStyleColors = 28;
static const int kNumStyleVars = 6;

static void PushNigelTheme() {
    // Vars
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 10.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_GrabRounding, 6.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarRounding, 8.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(16, 14));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(10, 7));

    // Colors — dark blue theme
    ImGui::PushStyleColor(ImGuiCol_Text,                rgba(230, 235, 240, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_TextDisabled,        rgba(120, 130, 140, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_WindowBg,            rgba(12, 14, 22, 0.96f));
    ImGui::PushStyleColor(ImGuiCol_Border,              rgba(45, 55, 80, 0.5f));
    ImGui::PushStyleColor(ImGuiCol_TitleBg,             rgba(18, 22, 36, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_TitleBgActive,       rgba(30, 42, 72, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_TitleBgCollapsed,    rgba(18, 22, 36, 0.8f));

    ImGui::PushStyleColor(ImGuiCol_FrameBg,             rgba(22, 26, 40, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered,      rgba(30, 36, 55, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive,       rgba(38, 44, 65, 1.0f));

    ImGui::PushStyleColor(ImGuiCol_Button,              rgba(35, 42, 65, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered,       rgba(55, 75, 140, 0.8f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,        rgba(65, 90, 170, 0.9f));

    ImGui::PushStyleColor(ImGuiCol_Header,              rgba(35, 45, 75, 0.7f));
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered,       rgba(50, 65, 110, 0.8f));
    ImGui::PushStyleColor(ImGuiCol_HeaderActive,        rgba(60, 80, 140, 0.9f));

    ImGui::PushStyleColor(ImGuiCol_ScrollbarBg,         rgba(18, 22, 36, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ScrollbarGrab,       rgba(50, 60, 90, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ScrollbarGrabHovered,rgba(65, 80, 120, 0.8f));
    ImGui::PushStyleColor(ImGuiCol_ScrollbarGrabActive, rgba(75, 95, 145, 0.9f));

    ImGui::PushStyleColor(ImGuiCol_CheckMark,           rgba(100, 160, 255, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_SliderGrab,          rgba(80, 130, 220, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_SliderGrabActive,    rgba(100, 155, 255, 1.0f));

    ImGui::PushStyleColor(ImGuiCol_Separator,           rgba(40, 50, 80, 0.5f));
    ImGui::PushStyleColor(ImGuiCol_TextSelectedBg,      rgba(60, 100, 200, 0.5f));

    ImGui::PushStyleColor(ImGuiCol_PopupBg,             rgba(18, 22, 36, 0.97f));
    ImGui::PushStyleColor(ImGuiCol_ResizeGrip,          rgba(55, 75, 140, 0.25f));
    ImGui::PushStyleColor(ImGuiCol_ResizeGripHovered,   rgba(65, 90, 170, 0.5f));
}

static void PopNigelTheme() {
    ImGui::PopStyleColor(kNumStyleColors);
    ImGui::PopStyleVar(kNumStyleVars);
}

// ---------------------------------------------------------------------------
// Labeled field helpers — label above, full-width input
// ---------------------------------------------------------------------------
static void FieldLabel(const char* label) {
    ImGui::TextDisabled("%s", label);
}

static void FieldInputText(const char* id, char* buf, size_t bufSize, float width = -1.0f) {
    if (width > 0) ImGui::SetNextItemWidth(width);
    else ImGui::SetNextItemWidth(-1);
    ImGui::InputText(id, buf, bufSize);
}

static void FieldInputInt(const char* id, int* val, float width = -1.0f) {
    if (width > 0) ImGui::SetNextItemWidth(width);
    else ImGui::SetNextItemWidth(-1);
    ImGui::InputInt(id, val, 0, 0);
}

// ---------------------------------------------------------------------------
// Status banner
// ---------------------------------------------------------------------------
static void DrawStatusBanner() {
    bool botOn = g_BotEnabled.load();
    bool modelOk = g_ModelOk.load();

    ImVec2 p = ImGui::GetCursorScreenPos();
    float w = ImGui::GetContentRegionAvail().x;
    float h = 36.0f;
    ImDrawList* dl = ImGui::GetWindowDrawList();

    ImU32 bannerCol;
    ImU32 dotCol;
    static char statusBuf[96];

    if (botOn && modelOk) {
        bannerCol = IM_COL32(0, 135, 80, 160);
        dotCol = IM_COL32(80, 255, 140, 255);
        snprintf(statusBuf, sizeof(statusBuf), "Active");
    } else if (modelOk) {
        bannerCol = IM_COL32(160, 125, 0, 150);
        dotCol = IM_COL32(255, 200, 50, 255);
        snprintf(statusBuf, sizeof(statusBuf), "Ready  -  press %s to activate", g_ToggleKey.c_str());
    } else {
        bannerCol = IM_COL32(52, 54, 62, 160);
        dotCol = IM_COL32(100, 102, 110, 255);
        snprintf(statusBuf, sizeof(statusBuf), "No model loaded");
    }

    dl->AddRectFilled(p, ImVec2(p.x + w, p.y + h), bannerCol, 6.0f);
    dl->AddCircleFilled(ImVec2(p.x + 14, p.y + h * 0.5f), 4.5f, dotCol);
    float textY = p.y + (h - ImGui::GetTextLineHeight()) * 0.5f;
    dl->AddText(ImVec2(p.x + 26, textY), IM_COL32(240, 242, 248, 240), statusBuf);
    ImGui::Dummy(ImVec2(w, h));
}

// ---------------------------------------------------------------------------
// Live inputs display
// ---------------------------------------------------------------------------
static void DrawLiveInputs() {
    bool active = g_BotEnabled.load() && g_ModelOk.load();
    if (!active) {
        ImGui::TextDisabled("Bot not active");
        return;
    }

    FVehicleInputs inp{};
    { std::lock_guard<std::mutex> lk(g_InputMx); inp = g_LatestInput; }

    char overlay[32];

    // Throttle bar
    snprintf(overlay, sizeof(overlay), "%.2f", inp.Throttle);
    ImGui::TextDisabled("Throttle");
    ImGui::PushStyleColor(ImGuiCol_PlotHistogram, rgba(80, 130, 220, 0.9f));
    ImGui::ProgressBar((inp.Throttle + 1.0f) * 0.5f, ImVec2(-1, 12), overlay);
    ImGui::PopStyleColor();

    // Steer bar
    snprintf(overlay, sizeof(overlay), "%.2f", inp.Steer);
    ImGui::TextDisabled("Steer");
    ImGui::PushStyleColor(ImGuiCol_PlotHistogram, rgba(80, 130, 220, 0.9f));
    ImGui::ProgressBar((inp.Steer + 1.0f) * 0.5f, ImVec2(-1, 12), overlay);
    ImGui::PopStyleColor();

    ImGui::Spacing();

    // Indicator dots for binary inputs
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 cp = ImGui::GetCursorScreenPos();
    float dotR = 4.5f;
    float rowH = 18.0f;
    ImU32 onCol = IM_COL32(100, 160, 255, 255);
    ImU32 offCol = IM_COL32(60, 62, 70, 255);
    ImU32 labelCol = IM_COL32(160, 162, 170, 255);

    struct Indicator { const char* name; bool on; };
    Indicator indicators[] = {
        { "Boost", (bool)inp.bActivateBoost },
        { "Jump",  (bool)inp.bJump },
        { "Drift", (bool)inp.bHandbrake },
    };

    float xOff = 6.0f;
    for (auto& ind : indicators) {
        dl->AddCircleFilled(ImVec2(cp.x + xOff, cp.y + rowH * 0.5f), dotR, ind.on ? onCol : offCol);
        dl->AddText(ImVec2(cp.x + xOff + 12, cp.y + 1), labelCol, ind.name);
        xOff += ImGui::CalcTextSize(ind.name).x + 28.0f;
    }
    ImGui::Dummy(ImVec2(0, rowH));

    // Pitch / Yaw / Roll as compact row
    ImGui::TextDisabled("Pitch %.2f   Yaw %.2f   Roll %.2f", inp.Pitch, inp.Yaw, inp.Roll);
}

// ---------------------------------------------------------------------------
// Load Bot action
// ---------------------------------------------------------------------------
static void DoLoadBot(SavedConfig& sc, const char* activationOptions[], int numActivations) {
    static unsigned long long s_lastLoad = 0;
    unsigned long long now = (unsigned long long)GetTickCount64();
    if (now - s_lastLoad < 150) return;
    s_lastLoad = now;

    nigelOut("load bot pressed");

    std::vector<int> pVec, shVec;
    parse_ints(sc.policySize, pVec);
    parse_ints(sc.sharedHeadSize, shVec);

    NigelConfig newCfg;
    newCfg.ToggleKey = sc.toggleKey;
    newCfg.BotType = sc.botType;
    newCfg.Device = sc.device[0] ? sc.device : "cuda";
    newCfg.UseSharedHead = sc.useSharedHead;
    newCfg.ObsBuilder = sc.obsBuilder;
    newCfg.ObsSize = sc.obsSize;
    newCfg.TickSkip = sc.tickSkip;
    newCfg.ActionDelay = sc.actionDelay;
    newCfg.PolicySize = pVec;
    if (sc.useSharedHead) newCfg.SharedHeadSize = shVec;
    int actIdx = (sc.activationIdx >= 0 && sc.activationIdx < numActivations) ? sc.activationIdx : 0;
    newCfg.Activation = activationOptions[actIdx];

    wchar_t dllPath[MAX_PATH]{0};
    HMODULE hm = nullptr;
    if (!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                            (LPCSTR)&nigel_unload_thr, &hm)) hm = nullptr;
    GetModuleFileNameW(hm, dllPath, MAX_PATH);
    std::filesystem::path dllP(dllPath);
    std::filesystem::path botDir = dllP.has_parent_path()
        ? (dllP.parent_path().parent_path() / "out" / "build" / "x64-Release" / "core" / "checkpoints")
        : std::filesystem::path("..\\out\\build\\x64-Release\\core\\checkpoints");
    newCfg.BotPath = botDir.wstring();

    char pbuf[512];
    sprintf_s(pbuf, "botdir=%ls", newCfg.BotPath.c_str());
    nigelOut(pbuf);

    std::error_code ec;
    if (!std::filesystem::exists(botDir, ec) || !std::filesystem::is_directory(botDir, ec)) {
        nigelOut("load pressed; bot folder not found");
    } else {
        nigelOut("bot folder ok; applying cfg");
        NigelBot::ApplyConfig(newCfg);
        nigelOut("calling Inference_Init");
        NigelBot::Inference_Init();
        nigelOut("Inference_Init done");

        g_BotEnabled = false;
        char m[128];
        sprintf_s(m, "bot loaded; toggle=%s", g_ToggleKey.c_str());
        nigelOut(m);
    }
}

// ===========================================================================
// NigelUI
// ===========================================================================
NigelUI::NigelUI(const std::string& name, const std::string& description, uint32_t states)
    : Module(name, description, states), showWindow(true) {}
NigelUI::~NigelUI() {}

void NigelUI::OnRender() {
    if (!showWindow) return;

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags &= ~ImGuiConfigFlags_NoMouseCursorChange;
    io.MouseDrawCursor = false;

    PushNigelTheme();

    ImGui::SetNextWindowSize(ImVec2(370, 540), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Nigel", &showWindow, ImGuiWindowFlags_NoCollapse)) {

        // =============================================================
        // Status banner
        // =============================================================
        DrawStatusBanner();
        ImGui::Spacing();

        // =============================================================
        // Quick toggles
        // =============================================================
        {
            static bool humanizerEnabled = false;

            bool botToggle = g_BotEnabled.load();
            if (ImGui::Checkbox("Enable Bot", &botToggle)) {
                g_BotEnabled.store(botToggle);
            }
            ImGui::SameLine(0, 24);
            ImGui::BeginDisabled();
            ImGui::Checkbox("Humanizer", &humanizerEnabled);
            ImGui::EndDisabled();
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
                ImGui::SetTooltip("Coming soon");
            }
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // =============================================================
        // Config
        // =============================================================
        static SavedConfig sc;
        static bool cfgLoaded = false;
        if (!cfgLoaded) { LoadConfigFile(sc); cfgLoaded = true; }

        static const char* activationOptions[] = {
            "relu", "leaky_relu", "tanh", "sigmoid", "elu", "selu", "gelu", "swish"
        };

        // --- Primary settings (always visible) ---
        {
            float halfW = (ImGui::GetContentRegionAvail().x - 10) * 0.5f;
            FieldLabel("Device");
            FieldInputText("##Device", sc.device, sizeof(sc.device), halfW);
            ImGui::SameLine(0, 10);
            FieldLabel("Toggle Key");
            FieldInputText("##ToggleKey", sc.toggleKey, sizeof(sc.toggleKey), halfW);
        }

        ImGui::Spacing();

        // --- Network ---
        if (ImGui::CollapsingHeader("Network", ImGuiTreeNodeFlags_DefaultOpen)) {
            FieldLabel("Bot Type");
            FieldInputText("##BotType", sc.botType, sizeof(sc.botType));
            FieldLabel("Policy Size");
            FieldInputText("##PolicySize", sc.policySize, sizeof(sc.policySize));
            FieldLabel("Activation");
            ImGui::SetNextItemWidth(-1);
            ImGui::Combo("##Activation", &sc.activationIdx, activationOptions, IM_ARRAYSIZE(activationOptions));
            ImGui::Checkbox("Use Shared Head", &sc.useSharedHead);
            if (sc.useSharedHead) {
                FieldLabel("Shared Head Size");
                FieldInputText("##SharedHeadSize", sc.sharedHeadSize, sizeof(sc.sharedHeadSize));
            }
        }

        // --- Observation ---
        if (ImGui::CollapsingHeader("Observation")) {
            FieldLabel("Obs Builder");
            FieldInputText("##ObsBuilder", sc.obsBuilder, sizeof(sc.obsBuilder));
            FieldLabel("Obs Size");
            FieldInputInt("##ObsSize", &sc.obsSize);
            FieldLabel("Tick Skip");
            FieldInputInt("##TickSkip", &sc.tickSkip);
            FieldLabel("Action Delay");
            FieldInputInt("##ActionDelay", &sc.actionDelay);
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // =============================================================
        // Action buttons
        // =============================================================

        // Load Bot — accent
        ImGui::PushStyleColor(ImGuiCol_Button, rgba(45, 65, 130, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, rgba(55, 80, 160, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, rgba(65, 90, 170, 1.0f));
        if (ImGui::Button("Load Bot", ImVec2(-1, 34.0f))) {
            DoLoadBot(sc, activationOptions, IM_ARRAYSIZE(activationOptions));
        }
        ImGui::PopStyleColor(3);

        ImGui::Spacing();

        if (ImGui::Button("Save Config", ImVec2(-1, 28.0f))) {
            if (SaveConfigFile(sc)) nigelOut("config saved");
            else nigelOut("config save failed");
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // =============================================================
        // Live inputs
        // =============================================================
        if (ImGui::CollapsingHeader("Live Inputs", ImGuiTreeNodeFlags_DefaultOpen)) {
            DrawLiveInputs();
        }

        // =============================================================
        // About (collapsed by default)
        // =============================================================
        if (ImGui::CollapsingHeader("About")) {
            ImGui::Spacing();
            ImGui::TextColored(rgba(100, 160, 255, 0.9f), "Nigel");
            ImGui::TextDisabled("Deep RL bot for Rocket League");
            ImGui::Spacing();
            ImGui::TextDisabled("Built on:");
            ImGui::TextUnformatted("  GigaLearnCPP / RLGymCPP / RocketSim");
            ImGui::TextUnformatted("  LibTorch (PyTorch C++)");
            ImGui::Spacing();
            ImGui::TextDisabled("Controls:");
            ImGui::TextUnformatted("  F1  - Toggle overlay");
            ImGui::TextUnformatted("  R   - Toggle bot on/off");
            ImGui::Spacing();

            ImGui::PushStyleColor(ImGuiCol_Button, rgba(140, 40, 40, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, rgba(180, 50, 50, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, rgba(120, 30, 30, 1.0f));
            if (ImGui::Button("Unload DLL", ImVec2(-1, 28.0f))) {
                if (!g_nigelUnloading.exchange(true)) {
                    showWindow = false;
                    HANDLE th = CreateThread(nullptr, 0, &nigel_unload_thr, nullptr, 0, nullptr);
                    if (th) CloseHandle(th);
                }
            }
            ImGui::PopStyleColor(3);
        }
    }
    ImGui::End();

    PopNigelTheme();
}
