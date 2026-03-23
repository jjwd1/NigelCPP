#pragma once
#include "../FrameworkTorch.h"
#include <torch/optim/sgd.h>
#include <torch/csrc/api/include/torch/nn/utils/convert_parameters.h>

namespace NGL {
	typedef torch::optim::SGDOptions MagSGDOptions;

	class MagSGD : public torch::optim::SGD {
	public:
		explicit MagSGD(std::vector<torch::Tensor> params, MagSGDOptions defaults)
			: SGD(params, defaults) {}

		torch::Tensor step(LossClosure closure = nullptr) override {
			RG_NO_GRAD;
			torch::Tensor loss = {};
			if (closure != nullptr) {
				at::AutoGradMode enable_grad(true);
				loss = closure();
			}

			float gradMag = 0;
			for (auto& group : this->param_groups())
				for (auto& param : group.params())
					if (param.grad().defined())
						gradMag += param.grad().detach().square().sum().cpu().item<float>();
			gradMag = sqrtf(gradMag);

			for (auto& group : this->param_groups()) {
				for (auto& param : group.params()) {
					if (!param.grad().defined()) continue;
					auto& gradSlice = param.mutable_grad();
					gradSlice /= gradMag;
				}
			}
			return SGD::step(closure);
		}
	};
}
