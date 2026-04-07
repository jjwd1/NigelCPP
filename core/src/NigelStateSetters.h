#pragma once
#include <RLGymCPP/StateSetters/StateSetter.h>
#include <RLGymCPP/CommonValues.h>
#include <RLGymCPP/Math.h>
#include <vector>

namespace RLGC {

	using RocketSim::Math::RandFloat;
	using RLGC::Math::RandVec;

	// Helper: collect arena cars into a vector for indexed access
	inline std::vector<Car*> GetCarsVec(Arena* arena) {
		std::vector<Car*> v(arena->_cars.begin(), arena->_cars.end());
		return v;
	}

	// Places ball on top of car, moving forward — ground dribble & flick practice
	class BallOnCarState : public StateSetter {
	public:
		void ResetArena(Arena* arena) override {
			arena->ResetToRandomKickoff();
			auto cars = GetCarsVec(arena);
			if (cars.empty()) return;

			float x = RandFloat(-2500, 2500);
			float y = RandFloat(-3500, 3500);
			float yaw = RandFloat((float)-M_PI, (float)M_PI);
			Vec forward = { cosf(yaw), sinf(yaw), 0 };
			float speed = RandFloat(600, 1200);

			CarState cs = {};
			cs.pos = { x, y, 17 };
			cs.vel = forward * speed;
			cs.angVel = { 0, 0, 0 };
			cs.rotMat = Angle(yaw, 0, 0).ToRotMat();
			cs.boost = RandFloat(30, 100);
			cs.isOnGround = true;
			cars[0]->SetState(cs);

			BallState bs = {};
			bs.pos = { x, y, 170 };
			bs.vel = cs.vel;
			bs.angVel = { 0, 0, 0 };
			arena->ball->SetState(bs);

			if (cars.size() > 1) {
				CarState opp = {};
				float side = (cars[1]->team == Team::BLUE) ? -1.0f : 1.0f;
				opp.pos = { RandFloat(-1000, 1000), side * 4500, 17 };
				opp.vel = { 0, 0, 0 };
				opp.rotMat = Angle(0, 0, 0).ToRotMat();
				opp.boost = 33;
				opp.isOnGround = true;
				cars[1]->SetState(opp);
			}
		}
	};

	// Car and ball both airborne near wall — air dribble practice
	// Ball and car on ground, both moving toward wall.
	// Ball rolls up the wall, car chases to pop it off for aerial play.
	class WallBallState : public StateSetter {
	public:
		void ResetArena(Arena* arena) override {
			arena->ResetToRandomKickoff();
			auto cars = GetCarsVec(arena);
			if (cars.empty()) return;

			// Pick a random side wall
			float side = (RandFloat(0, 1) > 0.5f) ? 1.0f : -1.0f;
			float y = RandFloat(-3000, 3000);

			// Ball on ground ahead of car, moving toward wall
			float ballSpeed = RandFloat(1700, 2500);
			float ballY = y + RandFloat(-300, 300);
			BallState bs = {};
			bs.pos = { side * RandFloat(2500, 3200), ballY, 93 };
			bs.vel = { side * ballSpeed, RandFloat(-200, 200), 0 };
			bs.angVel = { 0, 0, 0 };
			arena->ball->SetState(bs);

			// Car on ground behind ball, chasing toward wall
			float carSpeed = RandFloat(800, 1400);
			CarState cs = {};
			cs.pos = { side * RandFloat(1500, 2400), y, 17 };
			// Face toward ball
			float yaw = atan2f(ballY - y, bs.pos.x - cs.pos.x);
			cs.vel = { cosf(yaw) * carSpeed, sinf(yaw) * carSpeed, 0 };
			cs.angVel = { 0, 0, 0 };
			cs.rotMat = Angle(yaw, 0, 0).ToRotMat();
			cs.boost = RandFloat(50, 100);
			cs.isOnGround = true;
			cars[0]->SetState(cs);

			if (cars.size() > 1) {
				CarState opp = {};
				float oppSide = (cars[1]->team == Team::BLUE) ? -1.0f : 1.0f;
				opp.pos = { RandFloat(-500, 500), oppSide * 4500, 17 };
				opp.vel = { 0, 0, 0 };
				opp.rotMat = Angle(0, 0, 0).ToRotMat();
				opp.boost = 33;
				opp.isOnGround = true;
				cars[1]->SetState(opp);
			}
		}
	};

	// Ball floating in air, car on ground — aerial practice for loose balls.
	// The bot must jump, boost upward, and hit the ball.
	class LooseAerialBallState : public StateSetter {
	public:
		void ResetArena(Arena* arena) override {
			arena->ResetToRandomKickoff();
			auto cars = GetCarsVec(arena);
			if (cars.empty()) return;

			// Ball in the air at a reachable height
			float ballX = RandFloat(-2500, 2500);
			float ballY = RandFloat(-3000, 3000);
			float ballZ = RandFloat(400, 1200);
			// Ball drifting toward center + opponent goal, same as FlipResetSetup
			float ballVz = RandFloat(650, 1000);
			float ballDriftSpeed = RandFloat(700, 1300);
			float oppGoalY = (cars[0]->team == Team::BLUE) ? 5120.0f : -5120.0f;
			float dx = 0.0f - ballX;
			float dy = oppGoalY - ballY;
			float dLen = sqrtf(dx * dx + dy * dy);
			if (dLen > 0) { dx /= dLen; dy /= dLen; }

			BallState bs = {};
			bs.pos = { ballX, ballY, ballZ };
			bs.vel = { dx * ballDriftSpeed, dy * ballDriftSpeed, ballVz };
			bs.angVel = { 0, 0, 0 };
			arena->ball->SetState(bs);

			// Car on ground, some distance from ball, facing roughly toward it
			float carDist = RandFloat(800, 2000);
			float carAngle = RandFloat((float)-M_PI, (float)M_PI);
			float carX = ballX + cosf(carAngle) * carDist;
			float carY = ballY + sinf(carAngle) * carDist;
			// Clamp to field bounds
			carX = RS_CLAMP(carX, -3800.0f, 3800.0f);
			carY = RS_CLAMP(carY, -4800.0f, 4800.0f);

			// Face toward ball
			float yaw = atan2f(ballY - carY, ballX - carX);

			CarState cs = {};
			cs.pos = { carX, carY, 17 };
			float speed = RandFloat(200, 800);
			cs.vel = { cosf(yaw) * speed, sinf(yaw) * speed, 0 };
			cs.angVel = { 0, 0, 0 };
			cs.rotMat = Angle(yaw, 0, 0).ToRotMat();
			cs.boost = RandFloat(40, 100);
			cs.isOnGround = true;
			cars[0]->SetState(cs);

			if (cars.size() > 1) {
				CarState opp = {};
				float oppSide = (cars[1]->team == Team::BLUE) ? -1.0f : 1.0f;
				opp.pos = { RandFloat(-1000, 1000), oppSide * 4500, 17 };
				opp.vel = { 0, 0, 0 };
				opp.rotMat = Angle(0, 0, 0).ToRotMat();
				opp.boost = 33;
				opp.isOnGround = true;
				cars[1]->SetState(opp);
			}
		}
	};

	// Ball rolling toward car — catch & dribble practice
	class BallRollingToCarState : public StateSetter {
	public:
		void ResetArena(Arena* arena) override {
			arena->ResetToRandomKickoff();
			auto cars = GetCarsVec(arena);
			if (cars.empty()) return;

			float x = RandFloat(-2000, 2000);
			float y = RandFloat(-3000, 3000);
			float yaw = RandFloat((float)-M_PI, (float)M_PI);

			CarState cs = {};
			cs.pos = { x, y, 17 };
			cs.vel = { 0, 0, 0 };
			cs.rotMat = Angle(yaw, 0, 0).ToRotMat();
			cs.boost = RandFloat(30, 100);
			cs.isOnGround = true;
			cars[0]->SetState(cs);

			float ballDist = RandFloat(800, 1500);
			float ballAngle = yaw + RandFloat(-0.5f, 0.5f);
			float ballSpeed = RandFloat(400, 1200);

			BallState bs = {};
			bs.pos = { x + cosf(ballAngle) * ballDist, y + sinf(ballAngle) * ballDist, 93 };
			bs.vel = { -cosf(ballAngle) * ballSpeed, -sinf(ballAngle) * ballSpeed, 0 };
			bs.angVel = { 0, 0, 0 };
			arena->ball->SetState(bs);

			if (cars.size() > 1) {
				CarState opp = {};
				float oppSide = (cars[1]->team == Team::BLUE) ? -1.0f : 1.0f;
				opp.pos = { RandFloat(-1000, 1000), oppSide * 4500, 17 };
				opp.vel = { 0, 0, 0 };
				opp.rotMat = Angle(0, 0, 0).ToRotMat();
				opp.boost = 33;
				opp.isOnGround = true;
				cars[1]->SetState(opp);
			}
		}
	};

	// Car below ball in air, upside down, both moving upward.
	// Bot must touch ball with wheels to earn flip reset.
	class FlipResetSetup : public StateSetter {
	public:
		void ResetArena(Arena* arena) override {
			arena->ResetToRandomKickoff();
			auto cars = GetCarsVec(arena);
			if (cars.empty()) return;

			int airIdx = 0, groundIdx = 1;
			int airTeam = rand() % 2; // 0 = blue air, 1 = orange air
			for (int i = 0; i < (int)cars.size(); i++) {
				if ((cars[i]->team == Team::BLUE) == (airTeam == 0)) airIdx = i;
				else groundIdx = i;
			}
			if (cars.size() < 2) { airIdx = 0; groundIdx = -1; }

			float oppGoalY = (cars[airIdx]->team == Team::BLUE) ? 5120.0f : -5120.0f;
			float ownHalfSign = (cars[airIdx]->team == Team::BLUE) ? -1.0f : 1.0f;

			float x = RandFloat(-2000, 2000);
			float y = ownHalfSign * RandFloat(70, 1976);
			float ballHeight = RandFloat(891, 1218);

			// Car spawns behind ball (away from opponent goal) with some spread
			float radius = RandFloat(150, 338);
			Vec awayFromGoal = { x, y - oppGoalY, 0 };
			float awayLen = awayFromGoal.Length2D();
			if (awayLen > 0) { awayFromGoal.x /= awayLen; awayFromGoal.y /= awayLen; }
			float spread = RandFloat(-0.35f, 0.35f);
			float baseAngle = atan2f(awayFromGoal.y, awayFromGoal.x);
			float angle = baseAngle + spread;

			float ballVz = RandFloat(700, 1000);
			float ballDriftSpeed = RandFloat(1100, 1900);
			float dx = 0.0f - x;
			float dy = oppGoalY - y;
			float dLen = sqrtf(dx * dx + dy * dy);
			if (dLen > 0) { dx /= dLen; dy /= dLen; }
			BallState bs = {};
			bs.pos = { x, y, ballHeight };
			bs.vel = { dx * ballDriftSpeed, dy * ballDriftSpeed, ballVz };
			bs.angVel = { 0, 0, 0 };
			arena->ball->SetState(bs);

			float carOffset = RandFloat(203, 338);
			float roll = RandFloat((float)-M_PI, (float)M_PI);
			float basePitch = RandFloat(0.3f, 0.6f);
			float pitch = basePitch * cosf(roll);
			float yaw = atan2f(dy, dx);

			CarState cs = {};
			cs.pos = { x + radius * cosf(angle), y + radius * sinf(angle), ballHeight - carOffset };
			Vec carPos = cs.pos;
			Vec dirToBall = { bs.pos.x - carPos.x, bs.pos.y - carPos.y, bs.pos.z - carPos.z };
			float dirLen = dirToBall.Length();
			if (dirLen > 0) { dirToBall.x /= dirLen; dirToBall.y /= dirLen; dirToBall.z /= dirLen; }
			float speed = RandFloat(10, 50);
			float velScale = 0.95f;
			cs.vel = { bs.vel.x * velScale + dirToBall.x * speed, bs.vel.y * velScale + dirToBall.y * speed, bs.vel.z * velScale + dirToBall.z * speed };
			cs.angVel = { 0, 0, 0 };
			cs.rotMat = Angle(yaw, pitch, roll).ToRotMat();
			cs.boost = RandFloat(30, 80);
			cs.isOnGround = false;
			cs.hasJumped = true;
			cs.hasDoubleJumped = true;
			cs.hasFlipped = false;
			cars[airIdx]->SetState(cs);

			if (groundIdx >= 0) {
				CarState opp = {};
				float groundGoalY = (cars[groundIdx]->team == Team::BLUE) ? -5120.0f : 5120.0f;
				opp.pos = { RandFloat(-500, 500), groundGoalY * 0.88f, 17 };
				opp.vel = { 0, 0, 0 };
				opp.rotMat = Angle(0, 0, 0).ToRotMat();
				opp.boost = 33;
				opp.isOnGround = true;
				cars[groundIdx]->SetState(opp);
			}
		}
	};
}
