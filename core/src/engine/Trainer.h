#pragma once
#include "../Framework.h"
#include "../config/TrainConfig.h"
#include "../ppo/PPOLearner.h"
#include "../ppo/ExperienceBuffer.h"
#include "../stats/WelfordStat.h"
#include "../util/Report.h"
#include "../util/RenderSender.h"
#include "PolicyVersionManager.h"

namespace NGL {

	typedef std::function<RLGC::EnvCreateResult(int)> EnvCreateFn;
	typedef std::function<void(class Trainer*, const std::vector<RLGC::GameState>&, Report&)> StepCallbackFn;

	// Metrics snapshot for the UI
	struct MetricsSnapshot {
		uint64_t totalTimesteps = 0;
		uint64_t totalIterations = 0;
		float overallSPS = 0;
		float collectionSPS = 0;
		float consumptionSPS = 0;
		float avgStepReward = 0;
		float policyEntropy = 0;
		float policyLoss = 0;
		float criticLoss = 0;
		float klDivergence = 0;
		float clipFraction = 0;
		float policyUpdateMag = 0;
		float criticUpdateMag = 0;
		float collectionTime = 0;
		float consumptionTime = 0;
		float gaeTime = 0;
		float ppoLearnTime = 0;

		// Per-reward breakdown
		std::unordered_map<std::string, float> rewardBreakdown;

		// Custom skill metrics from step callback
		std::unordered_map<std::string, float> skillMetrics;
	};

	enum class TrainerState {
		INITIALIZING,
		RUNNING,
		PAUSED,
		STOPPED
	};

	class Trainer {
	public:
		TrainConfig config;
		PPOLearner* ppo = nullptr;
		PolicyVersionManager* versionMgr = nullptr;
		RenderSender* renderSender = nullptr;
		RLGC::EnvSet* envSet = nullptr;

		WelfordStat* returnStat = nullptr;
		BatchedWelfordStat* obsStat = nullptr;

		int obsSize = 0;
		int numActions = 0;

		uint64_t totalTimesteps = 0;
		uint64_t totalIterations = 0;

		StepCallbackFn stepCallback = nullptr;

		// State
		std::atomic<TrainerState> state = TrainerState::INITIALIZING;
		std::atomic<bool> saveRequested = false;
		std::atomic<bool> stopRequested = false;

		// Checkpoint events (set by training thread, consumed by UI thread)
		std::atomic<int64_t> lastSavedTimesteps = -1;   // set after Save()
		std::atomic<int64_t> lastLoadedTimesteps = -1;   // set after Load()

		// Latest metrics for UI
		MetricsSnapshot latestMetrics;
		std::mutex metricsMutex;

		Trainer(EnvCreateFn envCreateFn, TrainConfig config, StepCallbackFn stepCallback = nullptr);

		void Start();
		void RequestPause();
		void RequestResume();
		void RequestSave();
		void RequestStop();

		TrainerState GetState() const { return state.load(); }
		MetricsSnapshot GetLatestMetrics();

		// Reward weights (read/write from UI thread)
		struct RewardInfo { std::string name; float weight; };
		std::vector<RewardInfo> GetRewardWeights();
		void SetRewardWeight(const std::string& name, float weight);

		// Checkpoint management
		void Save();
		void Load();
		void SaveStats(std::filesystem::path path);
		void LoadStats(std::filesystem::path path);

		~Trainer();
	};
}
