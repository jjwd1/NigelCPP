#pragma once
#include "../Framework.h"

namespace NGL {
	struct AvgTracker {
		float total;
		uint64_t count;

		AvgTracker() { Reset(); }

		float Get() const {
			if (count > 0) return total / count;
			return 0;
		}

		void Add(float val) {
			if (!isnan(val)) { total += val; count++; }
		}

		AvgTracker& operator+=(float val) { Add(val); return *this; }

		void Add(float totalVal, uint64_t count) {
			if (!isnan(totalVal)) { total += totalVal; this->count += count; }
		}

		AvgTracker& operator+=(const AvgTracker& other) {
			Add(other.total, other.count);
			return *this;
		}

		void Reset() { total = 0; count = 0; }
	};

	struct MutAvgTracker : AvgTracker {
		std::mutex mut = {};

		MutAvgTracker() { Reset(); }

		float Get() { std::lock_guard<std::mutex> lock(mut); return AvgTracker::Get(); }

		void Add(float val) { std::lock_guard<std::mutex> lock(mut); AvgTracker::Add(val); }
		MutAvgTracker& operator+=(float val) { Add(val); return *this; }

		void Add(float totalVal, uint64_t count) { std::lock_guard<std::mutex> lock(mut); AvgTracker::Add(totalVal, count); }
		MutAvgTracker& operator+=(const MutAvgTracker& other) { Add(other.total, other.count); return *this; }

		void Reset() { std::lock_guard<std::mutex> lock(mut); AvgTracker::Reset(); }
	};
}
