#pragma once
#include "../FrameworkTorch.h"

namespace NGL {

	struct ExperienceTensors {
		torch::Tensor states, actions, logProbs, targetValues, actionMasks, advantages;

		auto begin() { return &states; }
		auto end() { return &advantages + 1; }
		auto begin() const { return &states; }
		auto end() const { return &advantages + 1; }
	};

	class ExperienceBuffer {
	public:
		torch::Device device;
		int seed;
		ExperienceTensors data;
		std::default_random_engine rng;

		ExperienceBuffer(int seed, torch::Device device);

		// Improved: pre-shuffle all data then return contiguous slices (zero-copy views)
		std::vector<ExperienceTensors> GetAllBatchesShuffled(int64_t batchSize, bool overbatching);
	};
}
