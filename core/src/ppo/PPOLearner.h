#pragma once
#include "ExperienceBuffer.h"
#include "Models.h"
#include "../util/Report.h"
#include "../util/Timer.h"
#include "../config/TrainConfig.h"

namespace NGL {

	class PPOLearner {
	public:
		ModelSet models = {};

		PPOConfig config;
		torch::Device device;

		PPOLearner(int obsSize, int numActions, PPOConfig config, torch::Device device);

		static void MakeModels(
			bool makeCritic,
			int obsSize, int numActions,
			PartialModelConfig sharedHeadConfig, PartialModelConfig policyConfig, PartialModelConfig criticConfig,
			torch::Device device,
			ModelSet& outModels
		);

		void InferActions(torch::Tensor obs, torch::Tensor actionMasks, torch::Tensor* outActions, torch::Tensor* outLogProbs, ModelSet* models = NULL);
		torch::Tensor InferCritic(torch::Tensor obs);

		static torch::Tensor InferPolicyProbsFromModels(
			ModelSet& models,
			torch::Tensor obs, torch::Tensor actionMasks,
			float temperature, bool halfPrec
		);
		static void InferActionsFromModels(
			ModelSet& models,
			torch::Tensor obs, torch::Tensor actionMasks,
			bool deterministic, float temperature, bool halfPrec,
			torch::Tensor* outActions, torch::Tensor* outLogProbs
		);

		void Learn(ExperienceBuffer& experience, Report& report, bool isFirstIteration);

		void SaveTo(std::filesystem::path folderPath);
		void LoadFrom(std::filesystem::path folderPath);
		void SetLearningRates(float policyLR, float criticLR);

		ModelSet GetPolicyModels();
	};
}
