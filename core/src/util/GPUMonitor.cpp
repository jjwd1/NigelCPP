#include "GPUMonitor.h"

#ifdef HAS_NVML
#include <nvml.h>
#endif

NGL::GPUMonitor::GPUMonitor() {
#ifdef HAS_NVML
	nvmlReturn_t result = nvmlInit();
	initialized = (result == NVML_SUCCESS);
#endif
}

NGL::GPUStatus NGL::GPUMonitor::GetStatus() {
	GPUStatus status = {};

#ifdef HAS_NVML
	if (!initialized) return status;

	nvmlDevice_t device;
	if (nvmlDeviceGetHandleByIndex(0, &device) != NVML_SUCCESS) return status;

	status.available = true;

	nvmlMemory_t mem;
	if (nvmlDeviceGetMemoryInfo(device, &mem) == NVML_SUCCESS) {
		status.vramUsedMB = (int)(mem.used / (1024 * 1024));
		status.vramTotalMB = (int)(mem.total / (1024 * 1024));
	}

	nvmlUtilization_t util;
	if (nvmlDeviceGetUtilizationRates(device, &util) == NVML_SUCCESS) {
		status.utilization = util.gpu;
	}

	unsigned int temp;
	if (nvmlDeviceGetTemperature(device, NVML_TEMPERATURE_GPU, &temp) == NVML_SUCCESS) {
		status.temperature = temp;
	}

	unsigned int power;
	if (nvmlDeviceGetPowerUsage(device, &power) == NVML_SUCCESS) {
		status.powerDraw = power / 1000.0f;
	}
#endif

	return status;
}

NGL::GPUMonitor::~GPUMonitor() {
#ifdef HAS_NVML
	if (initialized) nvmlShutdown();
#endif
}
