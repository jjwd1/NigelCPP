#pragma once
#include "../FrameworkTorch.h"

namespace NGL {
	namespace GAE {
		void Compute(
			torch::Tensor rews, torch::Tensor terminals, torch::Tensor valPreds, torch::Tensor truncValPreds,
			torch::Tensor& outAdvantages, torch::Tensor& outValues, torch::Tensor& outReturns, float& outRewClipPortion,
			float gamma = 0.99f, float lambda = 0.95f, float returnStd = 0, float clipRange = 10
		);
	}
}
