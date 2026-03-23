#include "ExperienceBuffer.h"

using namespace torch;

NGL::ExperienceBuffer::ExperienceBuffer(int seed, torch::Device device) :
	seed(seed), device(device), rng(seed) {
}

std::vector<NGL::ExperienceTensors> NGL::ExperienceBuffer::GetAllBatchesShuffled(int64_t batchSize, bool overbatching) {
	RG_NO_GRAD;

	int64_t expSize = data.states.size(0);

	// Pre-shuffle: generate a permutation and reindex ALL tensors once
	auto permIndices = torch::randperm(expSize, torch::kInt64);

	// Reindex all experience tensors using the permutation
	ExperienceTensors shuffled;
	auto* toItr = shuffled.begin();
	auto* fromItr = data.begin();
	for (; toItr != shuffled.end(); toItr++, fromItr++)
		*toItr = fromItr->index_select(0, permIndices);

	// Now slice into contiguous batches (zero-copy views via slice)
	std::vector<ExperienceTensors> result;
	for (int64_t startIdx = 0; startIdx + batchSize <= expSize; startIdx += batchSize) {

		int64_t curBatchSize = batchSize;
		if (startIdx + batchSize * 2 > expSize) {
			if (overbatching)
				curBatchSize = expSize - startIdx;
		}

		ExperienceTensors batch;
		auto* batchItr = batch.begin();
		auto* srcItr = shuffled.begin();
		for (; batchItr != batch.end(); batchItr++, srcItr++)
			*batchItr = srcItr->slice(0, startIdx, startIdx + curBatchSize);

		result.push_back(batch);
	}

	return result;
}
