#pragma once
#include "Framework.h"
#include <RLGymCPP/BasicTypes/Lists.h>

// Include torch
#include <ATen/ATen.h>
#include <ATen/autocast_mode.h>
#include <torch/utils.h>

#define RG_NO_GRAD torch::NoGradGuard _noGradGuard

#define RG_HALFPERC_TYPE torch::ScalarType::BFloat16

namespace NGL {
	template <typename T>
	inline torch::Tensor DIMLIST2_TO_TENSOR(const RLGC::DimList2<T>& list) {
		return torch::tensor(list.data).reshape({ (int64_t)list.size[0], (int64_t)list.size[1] });
	}

	// Pinned-memory variant: CPU tensor in page-locked memory for faster GPU transfers
	template <typename T>
	inline torch::Tensor DIMLIST2_TO_TENSOR_PINNED(const RLGC::DimList2<T>& list) {
		auto opts = torch::TensorOptions().dtype(torch::CppTypeToScalarType<T>()).pinned_memory(true);
		auto t = torch::empty({ (int64_t)(list.size[0] * list.size[1]) }, opts);
		memcpy(t.data_ptr<T>(), list.data.data(), list.data.size() * sizeof(T));
		return t.reshape({ (int64_t)list.size[0], (int64_t)list.size[1] });
	}

	template <typename T>
	inline std::vector<T> TENSOR_TO_VEC(torch::Tensor tensor) {
		assert(tensor.dim() == 1);
		tensor = tensor.contiguous().cpu().detach().to(torch::CppTypeToScalarType<T>());
		T* data = tensor.data_ptr<T>();
		return std::vector<T>(data, data + tensor.size(0));
	}
}
