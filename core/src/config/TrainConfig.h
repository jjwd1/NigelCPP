#pragma once
#include "../Framework.h"
#include "ModelConfig.h"

namespace NGL {

	enum class DeviceType {
		AUTO,
		CPU,
		GPU_CUDA
	};

	struct PPOConfig {
		int64_t tsPerItr = 50'000;
		int64_t batchSize = 50'000;
		int64_t miniBatchSize = 0;
		bool overbatching = true;

		double maxEpisodeDuration = 120;
		bool deterministic = false;
		bool useHalfPrecision = false;

		PartialModelConfig policy, critic, sharedHead;

		int epochs = 2;
		float policyLR = 3e-4f;
		float criticLR = 3e-4f;

		float entropyScale = 0.018f;
		bool maskEntropy = false;

		float clipRange = 0.2f;
		float policyTemperature = 1;

		float gaeLambda = 0.95f;
		float gaeGamma = 0.99f;
		float rewardClipRange = 10;

		PPOConfig() {
			policy = {};
			policy.layerSizes = { 256, 256, 256 };
			critic = {};
			critic.layerSizes = { 256, 256, 256 };
			sharedHead = {};
			sharedHead.layerSizes = { 256 };
			sharedHead.addOutputLayer = false;
		}
	};

	struct SkillTrackerConfig {
		bool enabled = false;
		int numArenas = 16;
		float simTime = 45;
		float maxSimTime = 240;
		int updateInterval = 16;
		float ratingInc = 5;
		float initialRating = 0;
		bool deterministic = false;
	};

	struct TrainConfig {
		int numGames = 300;
		int tickSkip = 8;
		int actionDelay = 7;

		bool renderMode = false;
		float renderTimeScale = 1.0f;

		PPOConfig ppo = {};

		std::filesystem::path checkpointFolder = "checkpoints";
		int64_t tsPerSave = 1'000'000;
		int64_t randomSeed = -1;
		int checkpointsToKeep = 8;
		DeviceType deviceType = DeviceType::AUTO;

		bool standardizeObs = false;
		float minObsSTD = 1 / 10.f;
		float maxObsMeanRange = 3;
		int maxObsSamples = 100;

		bool standardizeReturns = true;
		int maxReturnSamples = 150;

		bool addRewardsToMetrics = true;
		int maxRewardSamples = 50;
		int rewardSampleRandInterval = 8;

		bool savePolicyVersions = false;
		int64_t tsPerVersion = 25'000'000;
		int maxOldVersions = 32;

		bool trainAgainstOldVersions = false;
		float trainAgainstOldChance = 0.15f;

		SkillTrackerConfig skillTracker = {};
	};
}
