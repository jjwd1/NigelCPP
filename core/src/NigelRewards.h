#pragma once
#include <RLGymCPP/Rewards/Reward.h>
#include <RLGymCPP/CommonValues.h>
#include <RLGymCPP/Math.h>

namespace RLGC {

	// =========================================================================
	// Takeoff boost helper: tracks each player's boost at the moment they
	// leave the ground. Embed in any reward that needs to gate on takeoff boost.
	// =========================================================================
	struct TakeoffBoostTracker {
		static constexpr int MAX_PLAYERS = 2;
		float boostAtTakeoff[MAX_PLAYERS] = {};
		bool wasOnGround[MAX_PLAYERS] = { true, true };

		void Reset() {
			for (int p = 0; p < MAX_PLAYERS; p++) {
				boostAtTakeoff[p] = 0;
				wasOnGround[p] = true;
			}
		}

		void Update(const Player& player, int pIdx) {
			if (pIdx >= MAX_PLAYERS) pIdx = 0;
			if (player.isOnGround) {
				wasOnGround[pIdx] = true;
			} else if (wasOnGround[pIdx]) {
				boostAtTakeoff[pIdx] = player.boost;
				wasOnGround[pIdx] = false;
			}
		}

		float Get(int pIdx) const {
			if (pIdx >= MAX_PLAYERS) pIdx = 0;
			return boostAtTakeoff[pIdx];
		}
	};

	// =========================================================================
	// Ground dribble: ball balanced on/near car while driving on ground
	// =========================================================================
	// Helper: check if any opponent is also close to the ball (shared possession).
	// Used to prevent cooperative ball-sharing to farm dribble rewards.
	inline bool OpponentNearBall(const Player& player, const GameState& state, float maxDist = 400.0f) {
		for (auto& p : state.players) {
			if (p.team == player.team)
				continue;
			if (p.pos.Dist(state.ball.pos) < maxDist)
				return true;
		}
		return false;
	}

	class GroundDribbleReward : public Reward {
	public:
		virtual float GetReward(const Player& player, const GameState& state, bool isFinal) override {
			if (!player.isOnGround)
				return 0;

			Vec ballRelPos = state.ball.pos - player.pos;
			float horizDist = ballRelPos.Length2D();
			float vertDist = ballRelPos.z;

			if (horizDist > 250 || vertDist < 100 || vertDist > 300)
				return 0;

			if (OpponentNearBall(player, state, 250.0f))
				return 0;

			float velAlignment = 0;
			if (player.vel.Length() > 100 && state.ball.vel.Length() > 100) {
				velAlignment = player.vel.Normalized().Dot(state.ball.vel.Normalized());
				velAlignment = RS_MAX(0, velAlignment);
			}

			return (0.5f + 0.5f * velAlignment) * 1.1f;
		}
	};

	// =========================================================================
	// Steering smoothness: penalize spammy left-right input oscillation.
	// Tracks a 15-frame history of steer/yaw inputs and counts direction
	// changes. 5+ changes in 15 frames = spam. Works on ground AND air.
	// Also checks physical angular velocity oscillation.
	// =========================================================================
	class SteeringSmoothnessPenalty : public Reward {
	public:
		// Track last N steer/roll inputs to detect spammy oscillation patterns.
		// Per-player history (2 players per game) so inputs don't mix.
		static constexpr int HISTORY_SIZE = 15;
		static constexpr int MAX_PLAYERS = 2;
		float steerHistory[MAX_PLAYERS][HISTORY_SIZE] = {};
		float rollHistory[MAX_PLAYERS][HISTORY_SIZE] = {};
		float throttleHistory[MAX_PLAYERS][HISTORY_SIZE] = {};
		float pitchHistory[MAX_PLAYERS][HISTORY_SIZE] = {};
		int historyIndex[MAX_PLAYERS] = {};
		bool historyFull[MAX_PLAYERS] = {};

		virtual void Reset(const GameState& initialState) override {
			for (int p = 0; p < MAX_PLAYERS; p++) {
				for (int i = 0; i < HISTORY_SIZE; i++) {
					steerHistory[p][i] = 0;
					rollHistory[p][i] = 0;
					throttleHistory[p][i] = 0;
					pitchHistory[p][i] = 0;
				}
				historyIndex[p] = 0;
				historyFull[p] = false;
			}
		}

		int countDirectionChanges(float* history, int startIdx, int count, bool full) {
			int directionChanges = 0;
			float prevVal = 0;
			bool prevSet = false;

			for (int i = 0; i < count; i++) {
				int idx = full ? (startIdx + i) % HISTORY_SIZE : i;
				float val = history[idx];

				if (fabsf(val) < 0.1f)
					continue;

				if (prevSet && ((val > 0 && prevVal < 0) || (val < 0 && prevVal > 0))) {
					directionChanges++;
				}

				prevVal = val;
				prevSet = true;
			}
			return directionChanges;
		}

		virtual float GetReward(const Player& player, const GameState& state, bool isFinal) override {
			if (!player.prev)
				return 0;

			// Find this player's index in the game
			int pIdx = 0;
			for (int i = 0; i < (int)state.players.size(); i++) {
				if (&state.players[i] == &player) { pIdx = i; break; }
			}
			if (pIdx >= MAX_PLAYERS) pIdx = 0;

			// On ground: track steer. In air: track yaw.
			float curInput = player.isOnGround ? player.prevAction.steer : player.prevAction.yaw;
			float curRoll = player.isOnGround ? 0.0f : player.prevAction.roll;
			float curThrottle = player.prevAction.throttle;
			float curPitch = player.isOnGround ? 0.0f : player.prevAction.pitch;

			int idx = historyIndex[pIdx];
			steerHistory[pIdx][idx] = curInput;
			rollHistory[pIdx][idx] = curRoll;
			throttleHistory[pIdx][idx] = curThrottle;
			pitchHistory[pIdx][idx] = curPitch;
			historyIndex[pIdx] = (idx + 1) % HISTORY_SIZE;
			if (historyIndex[pIdx] == 0) historyFull[pIdx] = true;

			int count = historyFull[pIdx] ? HISTORY_SIZE : historyIndex[pIdx];
			if (count < 3)
				return 0;

			float penalty = 0;

			// Count direction changes over the history window for steer/yaw and roll.
			int steerChanges = countDirectionChanges(steerHistory[pIdx], historyIndex[pIdx], count, historyFull[pIdx]);
			int rollChanges = countDirectionChanges(rollHistory[pIdx], historyIndex[pIdx], count, historyFull[pIdx]);
			int throttleChanges = countDirectionChanges(throttleHistory[pIdx], historyIndex[pIdx], count, historyFull[pIdx]);
			int pitchChanges = countDirectionChanges(pitchHistory[pIdx], historyIndex[pIdx], count, historyFull[pIdx]);

			// 5+ direction changes in 15 frames = spamming
			if (steerChanges >= 5)
				penalty -= 0.3f * steerChanges;
			if (rollChanges >= 5)
				penalty -= 0.3f * rollChanges;
			if (throttleChanges >= 5)
				penalty -= 0.3f * throttleChanges;
			if (pitchChanges >= 5)
				penalty -= 0.3f * pitchChanges;

			return penalty;
		}
	};

	// =========================================================================
	// Flick: launch ball off car with a jump/flip while dribbling
	// Requires 10 out of last 20 frames of ball-on-car before the flick counts.
	// =========================================================================
	class FlickReward : public Reward {
	public:
		static constexpr int MAX_PLAYERS = 2;
		static constexpr int HISTORY_SIZE = 20;
		static constexpr int MIN_DRIBBLE_FRAMES = 10;
		static constexpr int MIN_BALL_AIRBORNE_FRAMES = 30;
		static constexpr float BALL_GROUND_Z = 120.0f;
		bool onCarHistory[MAX_PLAYERS][HISTORY_SIZE] = {};
		int historyIndex[MAX_PLAYERS] = {};
		int ballAirborneFrames[MAX_PLAYERS] = {};

		virtual void Reset(const GameState& initialState) override {
			for (int p = 0; p < MAX_PLAYERS; p++) {
				for (int i = 0; i < HISTORY_SIZE; i++) onCarHistory[p][i] = false;
				historyIndex[p] = 0;
				ballAirborneFrames[p] = 0;
			}
		}

		static bool BallOnCar(const Player& player, const GameState& state) {
			Vec ballRel = state.ball.pos - player.pos;
			return player.isOnGround && ballRel.Length2D() < 300 && ballRel.z > 40 && ballRel.z < 350;
		}

		virtual float GetReward(const Player& player, const GameState& state, bool isFinal) override {
			if (!player.prev || !state.prev)
				return 0;

			int pIdx = 0;
			for (int i = 0; i < (int)state.players.size(); i++) {
				if (&state.players[i] == &player) { pIdx = i; break; }
			}
			if (pIdx >= MAX_PLAYERS) pIdx = 0;

			// Track ball airborne (not on ground)
			if (state.ball.pos.z > BALL_GROUND_Z)
				ballAirborneFrames[pIdx]++;
			else
				ballAirborneFrames[pIdx] = 0;

			// Track ball-on-car for this frame
			onCarHistory[pIdx][historyIndex[pIdx]] = BallOnCar(player, state);
			historyIndex[pIdx] = (historyIndex[pIdx] + 1) % HISTORY_SIZE;

			// Count how many of last 20 frames had ball on car
			int onCarCount = 0;
			for (int i = 0; i < HISTORY_SIZE; i++)
				if (onCarHistory[pIdx][i]) onCarCount++;

			if (onCarCount < MIN_DRIBBLE_FRAMES)
				return 0;

			// Ball must have been off the ground for 2+ seconds
			if (ballAirborneFrames[pIdx] < MIN_BALL_AIRBORNE_FRAMES)
				return 0;

			// Did the player jump or flip?
			bool jumped = !player.isOnGround && player.prev->isOnGround;
			bool flipping = player.isFlipping;
			if (!jumped && !flipping)
				return 0;

			// Did the ball gain significant upward velocity?
			float ballUpVelGain = state.ball.vel.z - state.prev->ball.vel.z;
			if (ballUpVelGain < 200)
				return 0;

			// Scale by how much velocity the ball gained (better flick = more speed)
			float velGain = (state.ball.vel - state.prev->ball.vel).Length();
			float score = RS_MIN(1.0f, velGain / 1500.0f);

			// Bonus if ball is heading toward opponent goal
			Vec goalDir = (player.team == Team::BLUE) ?
				CommonValues::ORANGE_GOAL_BACK : CommonValues::BLUE_GOAL_BACK;
			Vec ballToGoal = (goalDir - state.ball.pos).Normalized();
			float goalAlignment = RS_MAX(0, ballToGoal.Dot(state.ball.vel.Normalized()));

			return score * (0.6f + 0.4f * goalAlignment);
		}
	};

	// =========================================================================
	// Air roll during air dribble: small reward for using air roll input
	// while in an air dribble state. Teaches tornado spins and car control
	// during aerial carries — looks stylish and improves car orientation.
	// =========================================================================
	class AirRollDribbleReward : public Reward {
	public:
		float maxDist;
		float minHeight;
		AirRollDribbleReward(float maxDist = 400.0f, float minHeight = 300.0f)
			: maxDist(maxDist), minHeight(minHeight) {}

		virtual float GetReward(const Player& player, const GameState& state, bool isFinal) override {
			if (player.isOnGround)
				return 0;

			if (player.pos.z < minHeight || state.ball.pos.z < minHeight)
				return 0;

			float dist = player.pos.Dist(state.ball.pos);
			if (dist > maxDist)
				return 0;

			// Must be actively rolling
			if (fabsf(player.prevAction.roll) < 0.5f)
				return 0;

			return 1.0f;
		}
	};

	// =========================================================================
	// Aerial touch: bonus for touching ball while both are high in the air
	// Rewards successive aerial touches more than the first
	// =========================================================================
	class AerialTouchReward : public Reward {
	public:
		static constexpr int MAX_PLAYERS = 2;
		static constexpr int COOLDOWN_STEPS = 23; // ~1.5 seconds at tickSkip=8

		float minHeight;
		TakeoffBoostTracker takeoffTracker;
		int cooldown[MAX_PLAYERS] = {};

		AerialTouchReward(float minHeight = 300.0f) : minHeight(minHeight) {}

		virtual void Reset(const GameState& initialState) override {
			takeoffTracker.Reset();
			for (int p = 0; p < MAX_PLAYERS; p++) cooldown[p] = 0;
		}

		virtual float GetReward(const Player& player, const GameState& state, bool isFinal) override {
			int pIdx = 0;
			for (int i = 0; i < (int)state.players.size(); i++) {
				if (&state.players[i] == &player) { pIdx = i; break; }
			}
			if (pIdx >= MAX_PLAYERS) pIdx = 0;
			takeoffTracker.Update(player, pIdx);

			if (cooldown[pIdx] > 0) cooldown[pIdx]--;

			if (!player.ballTouchedStep)
				return 0;

			if (player.isOnGround || state.ball.pos.z < minHeight)
				return 0;

			// No reward if took off with less than 30 boost
			if (takeoffTracker.Get(pIdx) < 30)
				return 0;

			if (cooldown[pIdx] > 0)
				return 0;

			cooldown[pIdx] = COOLDOWN_STEPS;

			// Scale by height — higher touches are harder and more rewarding
			float heightBonus = RS_MIN(1.0f, state.ball.pos.z / CommonValues::CEILING_Z);

			return 0.5f + 0.5f * heightBonus;
		}
	};

	// =========================================================================
	// Flip reset: regain flip by touching ball with wheels while airborne
	// =========================================================================
	// Helper: detect flip reset via transition (didn't have flip -> now has flip, airborne near ball)
	inline bool DetectFlipReset(const Player& player, const GameState& state) {
		if (!player.prev) return false;
		if (state.ball.pos.z <= 350 || player.pos.z <= 350) return false;
		if (player.pos.Dist(state.ball.pos) >= 300) return false;
		if (player.prev->HasFlipReset() || !player.HasFlipReset()) return false;
		for (auto& p : state.players)
			if (p.team != player.team && player.pos.Dist(p.pos) < 350.0f)
				return false;
		return true;
	}

	class FlipResetReward : public Reward {
	public:
		virtual float GetReward(const Player& player, const GameState& state, bool isFinal) override {
			return DetectFlipReset(player, state) ? 1.0f : 0.0f;
		}
	};

	// =========================================================================
	// Flip reset follow-up: reward using the regained flip (flipping after reset)
	// This creates the full flip-reset-into-shot sequence
	// =========================================================================
	class FlipResetFollowUpReward : public Reward {
	public:
		static constexpr int MAX_PLAYERS = 2;
		static constexpr int MAX_FOLLOWUP_TICKS = 15; // ~1 second at 15 steps/sec (tickSkip=8)
		bool hadFlipReset[MAX_PLAYERS] = {};
		int ticksSinceReset[MAX_PLAYERS] = {};

		virtual void Reset(const GameState& initialState) override {
			for (int p = 0; p < MAX_PLAYERS; p++) {
				hadFlipReset[p] = false;
				ticksSinceReset[p] = 0;
			}
		}

		virtual float GetReward(const Player& player, const GameState& state, bool isFinal) override {
			if (!player.prev)
				return 0;

			int pIdx = 0;
			for (int i = 0; i < (int)state.players.size(); i++) {
				if (&state.players[i] == &player) { pIdx = i; break; }
			}
			if (pIdx >= MAX_PLAYERS) pIdx = 0;

			bool gotResetNow = DetectFlipReset(player, state);

			if (gotResetNow) {
				hadFlipReset[pIdx] = true;
				ticksSinceReset[pIdx] = 0;
			}

			if (hadFlipReset[pIdx]) {
				ticksSinceReset[pIdx]++;

				if (!player.isOnGround && player.ballTouchedStep && (player.isFlipping || player.hasFlipped)) {
					hadFlipReset[pIdx] = false;
					return 1.0f;
				}

				if (ticksSinceReset[pIdx] > MAX_FOLLOWUP_TICKS || player.pos.z < 150) {
					hadFlipReset[pIdx] = false;
				}
			}

			return 0;
		}
	};

	// =========================================================================
	// Chained flip resets: escalating reward for getting multiple flip resets
	// in a single aerial play (within ~4 seconds of each other).
	// Double flip reset = big reward, triple = massive reward.
	// Teaches the bot to maintain aerial control after a reset and go for another.
	// =========================================================================
	class ChainedFlipResetReward : public Reward {
	public:
		static constexpr int MAX_PLAYERS = 2;
		static constexpr int CHAIN_WINDOW_TICKS = 60; // ~4 seconds at 15 steps/sec
		int chainCount[MAX_PLAYERS] = {};
		int ticksSinceLastReset[MAX_PLAYERS] = {};
		bool tracking[MAX_PLAYERS] = {};

		virtual void Reset(const GameState& initialState) override {
			for (int p = 0; p < MAX_PLAYERS; p++) {
				chainCount[p] = 0;
				ticksSinceLastReset[p] = 0;
				tracking[p] = false;
			}
		}

		virtual float GetReward(const Player& player, const GameState& state, bool isFinal) override {
			if (!player.prev)
				return 0;

			int pIdx = 0;
			for (int i = 0; i < (int)state.players.size(); i++) {
				if (&state.players[i] == &player) { pIdx = i; break; }
			}
			if (pIdx >= MAX_PLAYERS) pIdx = 0;

			bool gotResetNow = DetectFlipReset(player, state);

			if (tracking[pIdx]) {
				ticksSinceLastReset[pIdx]++;

				if (player.pos.z < 150 || ticksSinceLastReset[pIdx] > CHAIN_WINDOW_TICKS) {
					chainCount[pIdx] = 0;
					tracking[pIdx] = false;
				}
			}

			if (gotResetNow) {
				if (tracking[pIdx]) {
					chainCount[pIdx]++;
					ticksSinceLastReset[pIdx] = 0;
					return (float)chainCount[pIdx];
				} else {
					chainCount[pIdx] = 1;
					ticksSinceLastReset[pIdx] = 0;
					tracking[pIdx] = true;
					return 0;
				}
			}

			return 0;
		}
	};

	// =========================================================================
	// Flip reset goal: huge reward for scoring within 3 seconds of a flip reset.
	// Teaches the bot to convert flip resets into actual goals.
	// =========================================================================
	class FlipResetGoalReward : public Reward {
	public:
		static constexpr int MAX_PLAYERS = 2;
		static constexpr int GOAL_WINDOW_TICKS = 45; // ~3 seconds at 15 steps/sec (tickSkip=8)
		bool hadFlipReset[MAX_PLAYERS] = {};
		int ticksSinceReset[MAX_PLAYERS] = {};

		virtual void Reset(const GameState& initialState) override {
			for (int p = 0; p < MAX_PLAYERS; p++) {
				hadFlipReset[p] = false;
				ticksSinceReset[p] = 0;
			}
		}

		virtual float GetReward(const Player& player, const GameState& state, bool isFinal) override {
			if (!player.prev)
				return 0;

			int pIdx = 0;
			for (int i = 0; i < (int)state.players.size(); i++) {
				if (&state.players[i] == &player) { pIdx = i; break; }
			}
			if (pIdx >= MAX_PLAYERS) pIdx = 0;

			bool gotResetNow = DetectFlipReset(player, state);

			if (gotResetNow) {
				hadFlipReset[pIdx] = true;
				ticksSinceReset[pIdx] = 0;
			}

			if (hadFlipReset[pIdx]) {
				ticksSinceReset[pIdx]++;

				// Goal scored while we have an active flip reset window
				if (state.goalScored && isFinal) {
					// Check if this player's team scored
					float oppGoalY = (player.team == Team::BLUE) ? CommonValues::BACK_WALL_Y : -CommonValues::BACK_WALL_Y;
					bool weScored = (state.ball.pos.y * oppGoalY) > 0;
					if (weScored) {
						hadFlipReset[pIdx] = false;
						return 1.0f;
					}
				}

				// Window expired or landed
				if (ticksSinceReset[pIdx] > GOAL_WINDOW_TICKS || player.pos.z < 150) {
					hadFlipReset[pIdx] = false;
				}
			}

			return 0;
		}
	};

	// =========================================================================
	// Flip reset guide: only active in FlipResetSetup episodes.
	// Rewards wheel-to-ball alignment within 300 units. Stops after first touch.
	// =========================================================================
	class FlipResetGuideReward : public Reward {
	public:
		static constexpr int MAX_PLAYERS = 2;
		float wheelWeight;
		float inversionWeight;
		bool isFlipResetEpisode = false;
		bool touched[MAX_PLAYERS] = {};
		bool landed[MAX_PLAYERS] = {};

		FlipResetGuideReward(float wheelWeight = 15.0f, float inversionWeight = 1.0f)
			: wheelWeight(wheelWeight), inversionWeight(inversionWeight) {}

		virtual void Reset(const GameState& initialState) override {
			isFlipResetEpisode = false;
			for (int p = 0; p < MAX_PLAYERS; p++) { touched[p] = false; landed[p] = false; }
			for (auto& p : initialState.players) {
				if (!p.isOnGround && p.hasDoubleJumped) {
					isFlipResetEpisode = true;
					break;
				}
			}
		}

		virtual float GetReward(const Player& player, const GameState& state, bool isFinal) override {
			int pIdx = 0;
			for (int i = 0; i < (int)state.players.size(); i++) {
				if (&state.players[i] == &player) { pIdx = i; break; }
			}
			if (pIdx >= MAX_PLAYERS) pIdx = 0;

			if (!isFlipResetEpisode) return 0;
			if (touched[pIdx] || landed[pIdx]) return 0;
			if (player.ballTouchedStep) { touched[pIdx] = true; return 0; }
			if (player.isOnGround) { landed[pIdx] = true; return 0; }

			float dist = player.pos.Dist(state.ball.pos);
			if (dist > 250.0f) return 0;

			Vec toBall = state.ball.pos - player.pos;
			Vec toBallNorm = { toBall.x / dist, toBall.y / dist, toBall.z / dist };

			Vec wheels = { -player.rotMat.up.x, -player.rotMat.up.y, -player.rotMat.up.z };
			float dot = wheels.x * toBallNorm.x + wheels.y * toBallNorm.y + wheels.z * toBallNorm.z;

			// Continuous ramp: 0 at 180° (dot=-1) to 1.0 at 5° (dot=0.996)
			float wheelReward = RS_MIN(1.0f, (dot + 1.0f) / 1.996f);

			// Inversion bonus: guides car to not be right-side up
			float upZ = player.rotMat.up.z;
			float inversionReward = RS_MIN(1.0f, 1.0f - upZ);

			return wheelWeight * wheelReward + inversionWeight * inversionReward;
		}
	};

	// =========================================================================
	// Aerial possession: in air with ball nearby (encourages staying close to ball in air)
	// =========================================================================
	class AerialPossessionReward : public Reward {
	public:
		float possessionDist;
		TakeoffBoostTracker takeoffTracker;
		AerialPossessionReward(float possessionDist = 400.0f) : possessionDist(possessionDist) {}

		virtual void Reset(const GameState& initialState) override { takeoffTracker.Reset(); }

		virtual float GetReward(const Player& player, const GameState& state, bool isFinal) override {
			int pIdx = 0;
			for (int i = 0; i < (int)state.players.size(); i++) {
				if (&state.players[i] == &player) { pIdx = i; break; }
			}
			takeoffTracker.Update(player, pIdx);

			if (player.isOnGround)
				return 0;

			if (player.pos.z < 200 || state.ball.pos.z < 200)
				return 0;

			if (takeoffTracker.Get(pIdx) < 30)
				return 0;

			float dist = player.pos.Dist(state.ball.pos);
			if (dist > possessionDist)
				return 0;

			// Goal direction: more reward when near ball heading toward opponent goal
			float oppGoalY = (player.team == Team::BLUE) ? CommonValues::BACK_WALL_Y : -CommonValues::BACK_WALL_Y;
			Vec dirToGoal = (Vec(0, oppGoalY, state.ball.pos.z) - state.ball.pos).Normalized();
			float goalDot = state.ball.vel.Normalized().Dot(dirToGoal);
			float goalMult = 0.5f + 0.5f * RS_MAX(0.0f, goalDot); // 0.5 base, up to 1.0

			return goalMult;
		}
	};

	// =========================================================================
	// Controlled touch: gentle touches that keep the ball close (for dribbling)
	// =========================================================================
	class ControlledTouchReward : public Reward {
	public:
		static constexpr int MAX_PLAYERS = 2;
		static constexpr int COOLDOWN_STEPS = 23; // ~1.5 seconds at tickSkip=8

		int cooldown[MAX_PLAYERS] = {};

		virtual void Reset(const GameState& initialState) override {
			for (int p = 0; p < MAX_PLAYERS; p++) cooldown[p] = 0;
		}

		virtual float GetReward(const Player& player, const GameState& state, bool isFinal) override {
			int pIdx = 0;
			for (int i = 0; i < (int)state.players.size(); i++) {
				if (&state.players[i] == &player) { pIdx = i; break; }
			}
			if (pIdx >= MAX_PLAYERS) pIdx = 0;

			if (cooldown[pIdx] > 0) cooldown[pIdx]--;

			if (!state.prev || !player.ballTouchedStep)
				return 0;

			if (cooldown[pIdx] > 0)
				return 0;

			cooldown[pIdx] = COOLDOWN_STEPS;

			float ballSpeedChange = fabsf(state.ball.vel.Length() - state.prev->ball.vel.Length());
			float maxChange = 2000.0f;

			// Gentle touch = high reward, hard smash = low reward
			return 1.0f - RS_MIN(1.0f, ballSpeedChange / maxChange);
		}
	};

	// =========================================================================
	// Ball carry: ball close above car at any height (ground or air carry)
	// =========================================================================
	class BallCarryReward : public Reward {
	public:
		virtual float GetReward(const Player& player, const GameState& state, bool isFinal) override {
			Vec ballRelPos = state.ball.pos - player.pos;

			// Ball must be above the car
			float upDot = player.rotMat.up.Dot(ballRelPos);
			if (upDot < 50 || upDot > 350)
				return 0;

			// No reward if opponent is also on top of the ball
			if (OpponentNearBall(player, state, 300.0f))
				return 0;

			// Ball must be close horizontally relative to car orientation
			float rightDot = fabsf(player.rotMat.right.Dot(ballRelPos));
			float forwardDot = fabsf(player.rotMat.forward.Dot(ballRelPos));

			if (rightDot > 150 || forwardDot > 200)
				return 0;

			return 1.0f;
		}
	};

	// =========================================================================
	// Dribble toward goal: rewards moving ball toward opponent goal while carrying
	// This connects dribbles to actual scoring opportunities
	// =========================================================================
	class DribbleToGoalReward : public Reward {
	public:
		virtual float GetReward(const Player& player, const GameState& state, bool isFinal) override {
			if (player.rotMat.up.z < 0.966f)
				return 0;

			Vec ballRelPos = state.ball.pos - player.pos;
			float dist = ballRelPos.Length();
			if (dist > 400)
				return 0;

			if (ballRelPos.z < 40 || ballRelPos.z > 350)
				return 0;

			if (OpponentNearBall(player, state, 250.0f))
				return 0;

			if (player.vel.Length() < 200)
				return 0;

			Vec goalPos = (player.team == Team::BLUE) ?
				CommonValues::ORANGE_GOAL_BACK : CommonValues::BLUE_GOAL_BACK;
			Vec toGoal = (goalPos - player.pos).Normalized();
			float goalward = player.vel.Normalized().Dot(toGoal);

			if (goalward < 0)
				return 0;

			float closeness = 1.0f - RS_MIN(1.0f, dist / 400.0f);

			return goalward * closeness;
		}
	};

	// =========================================================================
	// Wall carry: reward for dribbling the ball up the wall.
	// Ball must be balanced on car + on wall + moving upward.
	// Bridges ground dribble → wall → aerial transition.
	// =========================================================================
	class WallCarryReward : public Reward {
	public:
		virtual float GetReward(const Player& player, const GameState& state, bool isFinal) override {
			// Must be on a wall surface
			if (!player.isOnGround || player.pos.z < 200)
				return 0;

			bool nearSideWall = fabsf(player.pos.x) > 3500;
			bool nearBackWall = fabsf(player.pos.y) > 4600;
			if (!nearSideWall && !nearBackWall)
				return 0;

			// Ball must be balanced on/near the car (same idea as ground dribble)
			Vec ballRel = state.ball.pos - player.pos;
			float ballDist = ballRel.Length();
			if (ballDist > 350)
				return 0;

			// Ball should be "above" the car relative to car's up direction
			float upDot = player.rotMat.up.Dot(ballRel);
			if (upDot < 40 || upDot > 300)
				return 0;

			// Must be moving upward on the wall (carrying ball up, not sitting still)
			if (player.vel.z < 100)
				return 0;

			// Scale by height (higher = closer to aerial transition) and speed
			float heightScore = RS_MIN(1.0f, player.pos.z / 1500.0f);
			float speedScore = RS_MIN(1.0f, player.vel.Length() / 1500.0f);

			return heightScore * 0.5f + speedScore * 0.5f;
		}
	};

	// =========================================================================
	// Flick when pressured: big bonus for flicking when opponent is close.
	// Teaches the bot to flick over diving opponents or toward goal under pressure.
	// =========================================================================
	class FlickWhenPressuredReward : public Reward {
	public:
		static constexpr int MAX_PLAYERS = 2;
		static constexpr int HISTORY_SIZE = 20;
		static constexpr int MIN_DRIBBLE_FRAMES = 10;
		bool onCarHistory[MAX_PLAYERS][HISTORY_SIZE] = {};
		int historyIndex[MAX_PLAYERS] = {};

		virtual void Reset(const GameState& initialState) override {
			for (int p = 0; p < MAX_PLAYERS; p++) {
				for (int i = 0; i < HISTORY_SIZE; i++) onCarHistory[p][i] = false;
				historyIndex[p] = 0;
			}
		}

		virtual float GetReward(const Player& player, const GameState& state, bool isFinal) override {
			if (!player.prev || !state.prev)
				return 0;

			int pIdx = 0;
			for (int i = 0; i < (int)state.players.size(); i++) {
				if (&state.players[i] == &player) { pIdx = i; break; }
			}
			if (pIdx >= MAX_PLAYERS) pIdx = 0;

			// Track ball-on-car for this frame
			onCarHistory[pIdx][historyIndex[pIdx]] = FlickReward::BallOnCar(player, state);
			historyIndex[pIdx] = (historyIndex[pIdx] + 1) % HISTORY_SIZE;

			// Count how many of last 20 frames had ball on car
			int onCarCount = 0;
			for (int i = 0; i < HISTORY_SIZE; i++)
				if (onCarHistory[pIdx][i]) onCarCount++;

			if (onCarCount < MIN_DRIBBLE_FRAMES)
				return 0;

			// Did the player jump or flip?
			bool jumped = !player.isOnGround && player.prev->isOnGround;
			if (!jumped && !player.isFlipping)
				return 0;

			// Did the ball gain upward velocity? (actual flick)
			float ballUpVelGain = state.ball.vel.z - state.prev->ball.vel.z;
			if (ballUpVelGain < 200)
				return 0;

			// Opponent must be close (< 1300) and rushing in
			float closestOppDist = 99999;
			float oppSpeedToward = 0;
			for (auto& p : state.players) {
				if (p.team == player.team)
					continue;
				float d = p.pos.Dist(player.pos);
				if (d < closestOppDist) {
					closestOppDist = d;
					Vec dirToMe = (player.pos - p.pos).Normalized();
					oppSpeedToward = p.vel.Dot(dirToMe);
				}
			}

			if (closestOppDist > 1300 || oppSpeedToward < 300)
				return 0;

			// Bonus: ball heading toward opponent goal after flick
			Vec goalDir = (player.team == Team::BLUE) ?
				CommonValues::ORANGE_GOAL_BACK : CommonValues::BLUE_GOAL_BACK;
			Vec ballToGoal = (goalDir - state.ball.pos).Normalized();
			float goalAlignment = RS_MAX(0, ballToGoal.Dot(state.ball.vel.Normalized()));

			float reward = 0.5f + 0.5f * goalAlignment;

			return reward;
		}
	};

	// =========================================================================
	// Go for aerial: rewards moving upward toward a ball that's high in the air.
	// Teaches the bot to actually jump/boost toward loose aerial balls instead
	// of watching them float overhead. Continuous reward that scales with
	// how well the bot is closing distance to an elevated ball.
	// =========================================================================
	class GoForAerialReward : public Reward {
	public:
		float minBallHeight;
		TakeoffBoostTracker takeoffTracker;
		GoForAerialReward(float minBallHeight = 400.0f) : minBallHeight(minBallHeight) {}

		virtual void Reset(const GameState& initialState) override { takeoffTracker.Reset(); }

		virtual float GetReward(const Player& player, const GameState& state, bool isFinal) override {
			int pIdx = 0;
			for (int i = 0; i < (int)state.players.size(); i++) {
				if (&state.players[i] == &player) { pIdx = i; break; }
			}
			takeoffTracker.Update(player, pIdx);

			// Ball must be meaningfully in the air
			if (state.ball.pos.z < minBallHeight)
				return 0;

			// Don't reward if already very close to ball in air
			float ballDist = player.pos.Dist(state.ball.pos);
			if (ballDist < 200 && !player.isOnGround)
				return 0;

			if (ballDist > 3000)
				return 0;

			Vec dirToBall = (state.ball.pos - player.pos).Normalized();
			float speedToBall = player.vel.Dot(dirToBall);
			if (speedToBall < 0)
				return 0;

			float fullReward = 0.5f;

			// On ground: 45% reward to encourage jumping toward high balls
			if (player.isOnGround)
				return fullReward * 0.45f;

			// Airborne: full reward, but need 30+ boost at takeoff
			if (takeoffTracker.Get(pIdx) < 30)
				return 0;

			return fullReward;
		}
	};

	// =========================================================================
	// Power shot: reward shots on goal that are fast, bonus for long range
	// =========================================================================
	class PowerShotReward : public Reward {
	public:
		float minBallSpeed; // minimum ball speed to qualify (uu/s)
		float maxBallSpeed; // ball speed for max reward
		PowerShotReward(float minBallSpeedKPH = 40.0f, float maxBallSpeedKPH = 130.0f)
			: minBallSpeed(RLGC::Math::KPHToVel(minBallSpeedKPH)),
			  maxBallSpeed(RLGC::Math::KPHToVel(maxBallSpeedKPH)) {}

		virtual float GetReward(const Player& player, const GameState& state, bool isFinal) override {
			if (!player.eventState.shot)
				return 0;

			float ballSpeed = state.ball.vel.Length();
			if (ballSpeed < minBallSpeed)
				return 0;

			float speedScore = RS_MIN(1.0f, ballSpeed / maxBallSpeed);

			// Distance bonus: shots from far away are more impressive
			float oppGoalY = (player.team == Team::BLUE) ? CommonValues::BACK_WALL_Y : -CommonValues::BACK_WALL_Y;
			float distToGoal = fabsf(oppGoalY - player.pos.y);
			float distScore = RS_MIN(1.0f, distToGoal / 8000.0f); // 0 at goal, 1.0 at ~8000 units

			return speedScore * (0.5f + 0.5f * distScore);
		}
	};

	// =========================================================================
	// Seek boost: reward moving toward the nearest available boost pad when
	// boost is low. Teaches the bot to grab boost on the way to plays instead
	// of driving right past pads.
	// =========================================================================
	class SeekBoostReward : public Reward {
	public:
		float boostThreshold;
		SeekBoostReward(float boostThreshold = 50.0f) : boostThreshold(boostThreshold) {}

		// Big boost pads have z=73, small pads z=70
		static bool IsBigPad(int index) {
			return CommonValues::BOOST_LOCATIONS[index].z > 71;
		}

		virtual float GetReward(const Player& player, const GameState& state, bool isFinal) override {
			// Only active when boost is low
			if (player.boost > boostThreshold)
				return 0;

			// Only on ground (don't distract from aerial play)
			if (!player.isOnGround)
				return 0;

			// Find nearest available pad, preferring big pads.
			// Big pads get their distance halved so the bot will go for a big pad
			// even if a small pad is somewhat closer.
			const auto& pads = state.boostPads;
			float bestScore = 99999;
			Vec bestPadPos = {};
			bool bestIsBig = false;
			bool found = false;

			for (int i = 0; i < CommonValues::BOOST_LOCATIONS_AMOUNT; i++) {
				if (!pads[i])
					continue;

				Vec padPos = CommonValues::BOOST_LOCATIONS[i];
				float dist = player.pos.Dist(padPos);
				bool big = IsBigPad(i);

				// Big pads appear "closer" — bot will detour for them
				float effectiveDist = big ? dist * 0.4f : dist;

				if (effectiveDist < bestScore) {
					bestScore = effectiveDist;
					bestPadPos = padPos;
					bestIsBig = big;
					found = true;
				}
			}

			if (!found)
				return 0;

			float realDist = player.pos.Dist(bestPadPos);
			if (realDist > 2500)
				return 0;

			// Reward moving toward the chosen pad
			Vec dirToPad = (bestPadPos - player.pos).Normalized();
			float speedToPad = player.vel.Dot(dirToPad);
			if (speedToPad < 0)
				return 0;

			float velScore = RS_MIN(1.0f, speedToPad / 1500.0f);
			float distScore = 1.0f - RS_MIN(1.0f, realDist / 2500.0f);

			// Scale by how low boost is — lower boost = stronger incentive
			float urgency = 1.0f - (player.boost / boostThreshold);

			// Big pad bonus — extra multiplier for going to a big pad
			float bigBonus = bestIsBig ? 1.5f : 1.0f;

			return velScore * distScore * urgency * bigBonus;
		}
	};

	// =========================================================================
	// Boost while dribbling: bonus for picking up boost pads while carrying
	// the ball. Teaches the bot to route through pads on the way to plays
	// instead of choosing between boost and ball control.
	// =========================================================================
	class BoostWhileDribblingReward : public Reward {
	public:
		virtual float GetReward(const Player& player, const GameState& state, bool isFinal) override {
			if (!player.prev)
				return 0;

			// Did boost increase this step? (picked up a pad)
			if (player.boost <= player.prev->boost)
				return 0;

			// Ball must be close (possession)
			float ballDist = player.pos.Dist(state.ball.pos);
			if (ballDist > 300)
				return 0;

			// Bot and ball should be moving in similar directions (carrying, not chasing)
			if (player.vel.Length() > 200 && state.ball.vel.Length() > 200) {
				float alignment = player.vel.Normalized().Dot(state.ball.vel.Normalized());
				if (alignment < 0.3f)
					return 0;
			}

			// Flat reward — the event (boost pickup while possessing ball) is what matters
			return 1.0f;
		}
	};

	// =========================================================================
	// Relaxed face ball: like FaceBallReward but with a dead zone.
	// No reward when already roughly facing the ball (within ~15 degrees).
	// Prevents obsessive micro-corrections that cause steering jitter.
	// =========================================================================
	class RelaxedFaceBallReward : public Reward {
	public:
		bool ballHit = false;

		virtual void Reset(const GameState& initialState) override {
			ballHit = false;
		}

		virtual float GetReward(const Player& player, const GameState& state, bool isFinal) override {
			if (!ballHit) {
				if (state.ball.vel.Length() > 100)
					ballHit = true;
				else
					return 0;
			}

			Vec dirToBall = (state.ball.pos - player.pos).Normalized();
			float dot = player.rotMat.forward.Dot(dirToBall);

			// Beyond ~80 degrees from ball (dot < 0.174) — no reward
			if (dot < 0.174f)
				return 0;

			// Within ~15 degrees (dot > 0.966) — max reward
			if (dot > 0.966f)
				return 1.0f;

			// Ramp from 0 at 80 degrees to 1.0 at 15 degrees
			return (dot - 0.174f) / (0.966f - 0.174f);
		}
	};

	// =========================================================================
	// Kickoff reward: velocity toward ball + boost penalty + first touch bonus.
	// Active only while ball is at center (kickoff). Both players rewarded
	// independently (no broken zero-sum).
	// =========================================================================
	class KickoffReward2 : public Reward {
	public:
		bool isKickoff = false;
		bool firstTouchDone = false;
		int ticks = 0;

		virtual void Reset(const GameState& initialState) override {
			isKickoff = initialState.ball.vel.Length() < 10.0f;
			firstTouchDone = false;
			ticks = 0;
		}

		virtual float GetReward(const Player& player, const GameState& state, bool isFinal) override {
			if (!isKickoff) return 0;

			float reward = 0;

			// Boost penalty: punish not boosting when below max speed
			if (ticks > 0) {
				float speed = player.vel.Length();
				if (player.prevAction.boost < 1.0f && speed < CommonValues::CAR_MAX_SPEED) {
					reward -= (1.0f - player.prevAction.boost);
				}
			}

			// Velocity toward ball (continuous incentive to drive at ball)
			Vec dirToBall = state.ball.pos - player.pos;
			float distToBall = dirToBall.Length();
			if (distToBall > 0) {
				float speedTowardBall = player.vel.Dot(dirToBall / distToBall);
				reward += RS_MAX(0.0f, speedTowardBall / CommonValues::CAR_MAX_SPEED);
			}

			// First touch bonus
			if (!firstTouchDone && player.ballTouchedStep) {
				firstTouchDone = true;
				reward += 3.0f;
			}

			// End kickoff once ball is moving
			if (state.ball.vel.Length() > 100)
				isKickoff = false;

			ticks++;
			return reward;
		}
	};

	// =========================================================================
	// Kickoff reward: flip early + touch ball fast. Two one-time bonuses.
	// =========================================================================
	class KickoffReward : public Reward {
	public:
		static constexpr int MAX_PLAYERS = 2;
		static constexpr int SUSTAIN_FRAMES = 10;

		bool isKickoff = false;
		bool ballHit = false;
		bool firstTouchAwarded = false;
		int ticksSinceKickoff[MAX_PLAYERS] = {};
		float savedBonus[MAX_PLAYERS] = {};
		int sustainLeft[MAX_PLAYERS] = {};

		virtual void Reset(const GameState& initialState) override {
			isKickoff = initialState.ball.vel.Length() < 10.0f;
			ballHit = false;
			firstTouchAwarded = false;
			for (int p = 0; p < MAX_PLAYERS; p++) {
				ticksSinceKickoff[p] = 0;
				savedBonus[p] = 0;
				sustainLeft[p] = 0;
			}
		}

		virtual float GetReward(const Player& player, const GameState& state, bool isFinal) override {
			if (!isKickoff) return 0;

			int pIdx = 0;
			for (int i = 0; i < (int)state.players.size(); i++) {
				if (&state.players[i] == &player) { pIdx = i; break; }
			}
			if (pIdx >= MAX_PLAYERS) pIdx = 0;

			ticksSinceKickoff[pIdx]++;

			// Detect ball hit
			if (!ballHit && state.ball.vel.Length() > 100)
				ballHit = true;

			if (!ballHit)
				return 0;

			// First touch: base reward + speed bonus for faster arrival
			if (!firstTouchAwarded && player.ballTouchedStep) {
				firstTouchAwarded = true;
				float speedBonus = RS_MAX(0.0f, 1.0f - (float)ticksSinceKickoff[pIdx] / 40.0f);
				savedBonus[pIdx] = 0.5f + 4.5f * speedBonus;
				sustainLeft[pIdx] = SUSTAIN_FRAMES;
			}

			if (sustainLeft[pIdx] > 0) {
				sustainLeft[pIdx]--;
				return savedBonus[pIdx];
			}

			return 0;
		}
	};

	// =========================================================================
	// Defensive positioning: reward being between ball and own goal when
	// the ball is in our half. Teaches shadow defense and rotation.
	// =========================================================================
	class DefensivePositioningReward : public Reward {
	public:
		virtual float GetReward(const Player& player, const GameState& state, bool isFinal) override {
			// Own goal center (on the ground)
			Vec ownGoal = (player.team == Team::BLUE) ?
				Vec(0, -CommonValues::BACK_WALL_Y, 0) :
				Vec(0, CommonValues::BACK_WALL_Y, 0);

			// Only active when ball is in our half
			bool ballInOwnHalf = (player.team == Team::BLUE) ?
				(state.ball.pos.y < 0) : (state.ball.pos.y > 0);
			if (!ballInOwnHalf)
				return 0;

			// How deep is the ball in our half (0 = midfield, 1 = on goal line)
			float depth = fabsf(state.ball.pos.y) / CommonValues::BACK_WALL_Y;

			// Direction from own goal to ball
			Vec goalToBall = (state.ball.pos - ownGoal);
			float goalToBallDist = goalToBall.Length();
			if (goalToBallDist < 1.0f) return 0;
			goalToBall = goalToBall * (1.0f / goalToBallDist);

			// Direction from own goal to player
			Vec goalToPlayer = (player.pos - ownGoal);
			float goalToPlayerDist = goalToPlayer.Length();
			if (goalToPlayerDist < 1.0f) return 0;
			goalToPlayer = goalToPlayer * (1.0f / goalToPlayerDist);

			// Alignment: how well player lines up between goal and ball (1 = perfect)
			float alignment = goalToBall.Dot(goalToPlayer);
			if (alignment < 0) return 0;

			// Player must be closer to goal than the ball (between ball and goal)
			if (goalToPlayerDist >= goalToBallDist)
				return 0;

			// Reward scales with alignment and depth
			return alignment * depth;
		}
	};

	// =========================================================================
	// Air dribble: reward sustained aerial ball carry toward the opponent goal.
	// Tracks carry state (ball close + above player + both airborne), awards
	// continuous per-frame reward, a one-shot bonus for sustained carries (8+
	// frames), and a big bonus for scoring off an air dribble.
	// =========================================================================
	class AirDribbleReward : public Reward {
	public:
		static constexpr int MAX_PLAYERS = 2;
		static constexpr int SUSTAINED_THRESHOLD = 8;    // frames to qualify as sustained carry
		static constexpr int GRACE_FRAMES = 3;            // brief interruption tolerance
		static constexpr int GOAL_WINDOW_TICKS = 45;      // ~3 seconds at tickSkip=8

		float minHeight;
		float maxDist;

		// Per-player carry state
		int carryFrames[MAX_PLAYERS] = {};
		bool sustainedCarryAwarded[MAX_PLAYERS] = {};
		bool hadAirDribble[MAX_PLAYERS] = {};
		int ticksSinceCarry[MAX_PLAYERS] = {};

		TakeoffBoostTracker takeoffTracker;

		AirDribbleReward(float minHeight = 250.0f, float maxDist = 300.0f)
			: minHeight(minHeight), maxDist(maxDist) {}

		virtual void Reset(const GameState& initialState) override {
			takeoffTracker.Reset();
			for (int p = 0; p < MAX_PLAYERS; p++) {
				carryFrames[p] = 0;
				sustainedCarryAwarded[p] = false;
				hadAirDribble[p] = false;
				ticksSinceCarry[p] = 0;
			}
		}

		virtual float GetReward(const Player& player, const GameState& state, bool isFinal) override {
			int pIdx = 0;
			for (int i = 0; i < (int)state.players.size(); i++) {
				if (&state.players[i] == &player) { pIdx = i; break; }
			}
			if (pIdx >= MAX_PLAYERS) pIdx = 0;

			takeoffTracker.Update(player, pIdx);

			float dist = player.pos.Dist(state.ball.pos);
			float ballRelZ = state.ball.pos.z - player.pos.z;

			// Carry state: airborne, both high enough, ball close and above, enough takeoff boost
			bool inCarryState =
				!player.isOnGround &&
				player.pos.z > minHeight &&
				state.ball.pos.z > minHeight &&
				dist < maxDist &&
				ballRelZ > 0 &&
				takeoffTracker.Get(pIdx) >= 30;

			float reward = 0;

			// --- Carry frame tracking ---
			if (inCarryState) {
				carryFrames[pIdx]++;
				ticksSinceCarry[pIdx] = 0;

				// 4a: Continuous carry reward
				float maxHeightForBonus = CommonValues::CEILING_Z - 200.0f;
				float heightBonus = 1.0f + 0.3f * RS_MIN(1.0f, (player.pos.z - minHeight) / (maxHeightForBonus - minHeight));

				float oppGoalY = (player.team == Team::BLUE) ? CommonValues::BACK_WALL_Y : -CommonValues::BACK_WALL_Y;
				Vec dirToGoal = (Vec(0, oppGoalY, state.ball.pos.z) - state.ball.pos).Normalized();
				float goalDot = state.ball.vel.Normalized().Dot(dirToGoal);
				float goalMult = 0.6f + 0.4f * RS_MAX(0.0f, goalDot);

				reward += heightBonus * goalMult;

				// 4b: Sustained carry bonus (one-shot at 8 frames)
				if (carryFrames[pIdx] == SUSTAINED_THRESHOLD && !sustainedCarryAwarded[pIdx]) {
					sustainedCarryAwarded[pIdx] = true;
					hadAirDribble[pIdx] = true;
					reward += 3.0f;
				}
			} else {
				ticksSinceCarry[pIdx]++;
				if (ticksSinceCarry[pIdx] > GRACE_FRAMES || player.isOnGround || player.pos.z < 150) {
					carryFrames[pIdx] = 0;
					sustainedCarryAwarded[pIdx] = false;
				}
			}

			// 4c: Air dribble goal bonus (check BEFORE window-clear so landing doesn't prevent it)
			if (hadAirDribble[pIdx] && state.goalScored && isFinal) {
				float oppGoalY = (player.team == Team::BLUE) ? CommonValues::BACK_WALL_Y : -CommonValues::BACK_WALL_Y;
				bool weScored = (state.ball.pos.y * oppGoalY) > 0;
				if (weScored) {
					hadAirDribble[pIdx] = false;
					reward += 8.0f;
				}
			}

			// Clear goal window if too long since carry or landed
			if (ticksSinceCarry[pIdx] > GOAL_WINDOW_TICKS || player.pos.z < 150) {
				hadAirDribble[pIdx] = false;
			}

			return reward;
		}
	};

	// =========================================================================
	// Air roll: reward holding air roll right while airborne and high enough.
	// =========================================================================
	class AirRollReward : public Reward {
	public:
		virtual float GetReward(const Player& player, const GameState& state, bool isFinal) override {
			if (player.isOnGround || player.pos.z < 250)
				return 0;
			return RS_MAX(0.0f, player.prevAction.roll);
		}
	};
}
