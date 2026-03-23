#pragma once
#include <string>

namespace NGL {
	struct GPUStatus {
		int vramUsedMB = 0;
		int vramTotalMB = 0;
		int utilization = 0;
		int temperature = 0;
		float powerDraw = 0;
		bool available = false;
		std::string gpuName;
	};

	class GPUMonitor {
	public:
		bool initialized = false;

		GPUMonitor();
		GPUStatus GetStatus();
		~GPUMonitor();
	};
}
