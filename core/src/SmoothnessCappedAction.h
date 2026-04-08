#pragma once
#include <RLGymCPP/ActionParsers/DefaultAction.h>
#include <RLGymCPP/Gamestates/GameState.h>
#include <unordered_map>
#include <cmath>

namespace RLGC {

	// Physically caps per-frame input value changes in any 1-second window.
	// Holding a constant value is always free; every change in the raw value
	// counts as one input change — regardless of direction, magnitude, or
	// whether it crosses zero.
	//
	// Counted examples:
	//   [0.5, 0.5, 0.5, 0.5]            = 0 changes  (hold — always allowed)
	//   [1.0, 0.0, 1.0, 0.0]            = 3 changes  (tap on/off)
	//   [1.0, -1.0, 1.0, -1.0]          = 3 changes  (direction reversal)
	//   [0.25, 0.75, 0.25, 0.75]        = 3 changes  (magnitude oscillation)
	//   [0, 0.25, 0.5, 0.75, 1.0]       = 4 changes  (smooth ramp up)
	//
	// Trigger thresholds (over HISTORY_SIZE frames = 1 second at tickSkip=8):
	//   throttle, steer, pitch, yaw, roll, jump, handbrake: block at >= 7  (max 6 allowed)
	//   boost:                                               block at >= 11 (max 10 allowed)
	//
	// Matches SteeringSmoothnessPenalty's trigger points exactly, so the
	// penalty can never fire.
	//
	// When the threshold would be reached, that single input field is held at
	// its previous-frame value so the window stays at exactly threshold-1.
	class SmoothnessCappedAction : public DefaultAction {
	public:
		static constexpr int HISTORY_SIZE = 15;
		// These are the penalty's firing thresholds. We block at >= these values,
		// so the penalty can never fire.
		static constexpr int CONT_PENALTY_TRIGGER = 7;   // penalty fires at >= 7 sign flips
		static constexpr int BIN_PENALTY_TRIGGER = 7;    // penalty fires at >= 7 toggles
		static constexpr int BOOST_PENALTY_TRIGGER = 11; // penalty fires at >= 11 boost toggles

		struct PlayerHistory {
			float values[Action::ELEM_AMOUNT][HISTORY_SIZE] = {};
			int count = 0;    // number of valid entries (up to HISTORY_SIZE)
			int writeIdx = 0; // next write position in the ring
		};

		std::unordered_map<uint32_t, PlayerHistory> histories;

		// Counts any change in the raw input value between consecutive frames.
		// Holding produces 0 changes; every distinct value transition counts as 1,
		// whether it's a direction reversal, a tap-off, or a magnitude swing within
		// the same direction. Works identically for continuous and binary inputs —
		// binary (0/1) values compare the same way as discrete continuous values.
		static int CountValueChangesLinear(const float* arr, int len) {
			int changes = 0;
			float prevVal = 0;
			bool prevSet = false;
			for (int i = 0; i < len; i++) {
				float v = arr[i];
				if (prevSet && v != prevVal) changes++;
				prevVal = v;
				prevSet = true;
			}
			return changes;
		}

		// Build the chronological window the ring WOULD contain after appending newVal,
		// dropping the oldest entry if the ring was already full. Returns the new length.
		static int BuildCandidateWindow(const float* buf, int writeIdx, int count, float newVal, float* out) {
			if (count < HISTORY_SIZE) {
				for (int i = 0; i < count; i++) out[i] = buf[i];
				out[count] = newVal;
				return count + 1;
			} else {
				for (int i = 0; i < HISTORY_SIZE - 1; i++)
					out[i] = buf[(writeIdx + 1 + i) % HISTORY_SIZE];
				out[HISTORY_SIZE - 1] = newVal;
				return HISTORY_SIZE;
			}
		}

		virtual Action ParseAction(int index, const Player& player, const GameState& state) override {
			Action action = DefaultAction::ParseAction(index, player, state);
			auto& hist = histories[player.carId];

			// First step of a new episode: clear stale history so cross-episode
			// carryover doesn't constrain a freshly reset car.
			if (!player.prev) {
				hist.count = 0;
				hist.writeIdx = 0;
			}

			for (int i = 0; i < (int)Action::ELEM_AMOUNT; i++) {
				// i: 0=throttle 1=steer 2=pitch 3=yaw 4=roll 5=jump 6=boost 7=handbrake
				int trigger = (i == 6) ? BOOST_PENALTY_TRIGGER
				            : (i >= 5) ? BIN_PENALTY_TRIGGER
				                       : CONT_PENALTY_TRIGGER;

				float candidate[HISTORY_SIZE];
				int len = BuildCandidateWindow(hist.values[i], hist.writeIdx, hist.count, action[i], candidate);
				int changes = CountValueChangesLinear(candidate, len);

				if (changes >= trigger) {
					// Hold the previous frame's value for this field so no new
					// change occurs. count>0 is guaranteed here: an empty ring
					// yields len==1 with 0 changes, which never reaches any
					// positive trigger.
					int lastIdx = (hist.writeIdx - 1 + HISTORY_SIZE) % HISTORY_SIZE;
					action[i] = hist.values[i][lastIdx];
				}

				// Commit the final (possibly overridden) value to the ring
				hist.values[i][hist.writeIdx] = action[i];
			}

			hist.writeIdx = (hist.writeIdx + 1) % HISTORY_SIZE;
			if (hist.count < HISTORY_SIZE) hist.count++;

			return action;
		}
	};

}
