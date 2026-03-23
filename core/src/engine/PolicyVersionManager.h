#pragma once
#include "../ppo/Models.h"
#include "../config/TrainConfig.h"
#include "../util/Report.h"
#include <nlohmann/json.hpp>

namespace NGL {

	struct SkillRating {
		std::map<std::string, float> data;

		static std::string GetModeName(const RLGC::GameState& state) {
			int playersOnTeams[2] = { 0, 0 };
			for (auto& player : state.players) playersOnTeams[(int)player.team]++;
			int minP = RS_MIN(playersOnTeams[0], playersOnTeams[1]);
			int maxP = RS_MAX(playersOnTeams[0], playersOnTeams[1]);
			return RS_STR(minP << "v" << maxP);
		}

		float& GetRating(std::string name, float defaultRating) {
			if (data.contains(name)) return data[name];
			data[name] = defaultRating;
			return data[name];
		}

		float& GetRating(const RLGC::GameState& state, float defaultRating) {
			return GetRating(GetModeName(state), defaultRating);
		}

		nlohmann::json ToJSON() {
			nlohmann::json j = {};
			for (auto pair : data) j[pair.first] = pair.second;
			return j;
		}

		void ReadFromJSON(const nlohmann::json& j) {
			data = {};
			for (auto& pair : j.items()) data[pair.key()] = pair.value();
		}
	};

	struct PolicyVersion {
		uint64_t timesteps;
		ModelSet models;
		SkillRating ratings;
	};

	struct PolicyVersionManager {
		std::vector<PolicyVersion> versions;
		std::filesystem::path saveFolder;
		int maxVersions;
		uint64_t tsPerVersion;

		struct {
			SkillTrackerConfig config;
			RLGC::EnvSet* envSet;
			int curGoals = 0;
			bool doContinuation = false;
			int prevOldVersionIndex;
			Team prevNewTeam;
			float prevSimTime;
			int iterationsSinceRan = 0;
			SkillRating curRatings = {};
		} skill;

		PolicyVersionManager(
			std::filesystem::path saveFolder, int maxVersions, uint64_t tsPerVersion,
			const SkillTrackerConfig& skillTrackerConfig, const RLGC::EnvSetConfig& envSetConfig);

		PolicyVersion& AddVersion(ModelSet modelsToClone, uint64_t timesteps);
		void SaveVersions();
		void LoadVersions(ModelSet modelsTemplate, uint64_t curTimesteps);
		void SortVersions();
		void RunSkillMatches(class PPOLearner* ppo, Report& report);
		void OnIteration(class PPOLearner* ppo, Report& report, int64_t totalTimesteps, int64_t prevTotalTimesteps);
		void AddRunningStatsToJSON(nlohmann::json& json);
		void LoadRunningStatsFromJSON(const nlohmann::json& json);
	};
}
