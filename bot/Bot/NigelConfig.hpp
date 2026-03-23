#pragma once
#include <string>
#include <vector>

struct NigelConfig{
	std::string ToggleKey="R";
	std::string BotType="GGL_PPO";
	std::vector<int> PolicySize{1024};
	bool UseSharedHead=true;
	std::vector<int> SharedHeadSize{1024,1024,1024};
	std::vector<int> CriticSize{};
	std::wstring BotPath{};
	bool LayerNorm=true;

	std::string Activation="relu";
	std::string ObsBuilder="AdvancedObs";
	int ObsSize=109;
	int TickSkip=4;
	int ActionDelay=7;
	std::string Device="cuda";
};
