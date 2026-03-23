#include <QApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQmlError>
#include <QQuickStyle>
#include <QIcon>

#include "TrainingBridge.h"
#include "NigelRewards.h"
#include "NigelStateSetters.h"

#include <RLGymCPP/Rewards/CommonRewards.h>
#include <RLGymCPP/Rewards/ZeroSumReward.h>
#include <RLGymCPP/TerminalConditions/NoTouchCondition.h>
#include <RLGymCPP/TerminalConditions/GoalScoreCondition.h>
#include <RLGymCPP/OBSBuilders/DefaultObs.h>
#include <RLGymCPP/OBSBuilders/AdvancedObs.h>
#include <RLGymCPP/StateSetters/KickoffState.h>
#include <RLGymCPP/StateSetters/RandomState.h>
#include <RLGymCPP/StateSetters/CombinedState.h>
#include <RLGymCPP/StateSetters/FuzzedKickoffState.h>
#include <RLGymCPP/ActionParsers/DefaultAction.h>

using namespace NGL;
using namespace RLGC;

RLGC::EnvCreateResult EnvCreateFunc(int index) {
	std::vector<WeightedReward> rewards = {
		{ new VelocityPlayerToBallReward(), 0.15f },
		{ new AirReward(), 0.168f },
		{ new KickoffReward(), 10.0f },
		{ new PickupBoostReward(), 2.2f },
		{ new GoalReward(), 450.0f },
		{ new StrongTouchReward(20, 100), 1.3f },
		{ new ZeroSumReward(new VelocityBallToGoalReward(), 1.0f), 1.5f },
		{ new ShotReward(), 2.4f },
		{ new SaveReward(), 2.4f },
		{ new SteeringSmoothnessPenalty(), 2.0f },
		{ new DefensivePositioningReward(), 0.15f },
		{ new GroundDribbleReward(), 0.8f },
		{ new FlickReward(), 20.0f },
		{ new PowerShotReward(40, 130), 4.48f },
		{ new FlickWhenPressuredReward(), 5.0f },
		{ new ZeroSumReward(new BumpReward(), 0.5f), 0.75f },
		{ new WallToAirReward(), 18.0f },
		{ new GoForAerialReward(300), 9.6f },
		{ new AerialTouchReward(200), 36.0f },
		{ new AirDribbleReward(300, 250), 1.44f },
		{ new ChainedAerialTouchReward(), 12.0f },
		{ new AirRollDribbleReward(), 0.288f },
		{ new FaceBallInAirReward(250), 0.36f },
	};

	std::vector<TerminalCondition*> terminalConditions = {
		new NoTouchCondition(10),
		new GoalScoreCondition()
	};

	auto stateSetter = new CombinedState({
		{ new RandomState(true, true, true), 40.0f },
		{ new KickoffState(), 20.0f },
		{ new FuzzedKickoffState(), 5.0f },
		{ new BallOnCarState(), 5.0f },
		{ new WallBallState(), 5.0f },
		{ new LooseAerialBallState(), 10.0f },
		{ new AirDribbleSetup(), 15.0f },
	});

	int playersPerTeam = 1;
	auto arena = Arena::Create(GameMode::SOCCAR);
	for (int i = 0; i < playersPerTeam; i++) {
		arena->AddCar(Team::BLUE);
		arena->AddCar(Team::ORANGE);
	}

	EnvCreateResult result = {};
	result.actionParser = new DefaultAction();
	result.obsBuilder = new AdvancedObs();
	result.stateSetter = stateSetter;
	result.terminalConditions = terminalConditions;
	result.rewards = rewards;
	result.arena = arena;
	return result;
}

void StepCallback(Trainer* trainer, const std::vector<GameState>& states, Report& report) {
	bool doExpensiveMetrics = (rand() % 4) == 0;

	for (auto& state : states) {
		if (doExpensiveMetrics) {
			for (auto& player : state.players) {
				report.AddAvg("Player/In Air Ratio", !player.isOnGround);
				report.AddAvg("Player/Ball Touch Ratio", player.ballTouchedStep);
				report.AddAvg("Player/Speed", player.vel.Length());
				report.AddAvg("Player/Boost", player.boost);

				if (player.ballTouchedStep) {
					report.AddAvg("Player/Touch Height", state.ball.pos.z);
					report.AddAvg("Player/Touch Ball Speed", state.ball.vel.Length());
				}

				float ballDist = player.pos.Dist(state.ball.pos);
				Vec ballRel = state.ball.pos - player.pos;
				bool ballAbove = (ballRel.z > 60 && ballRel.z < 300);
				float horizDist = ballRel.Length2D();

				report.AddAvg("Nigel/Near Ball Ratio", ballDist < 500);

				bool groundDribble = player.isOnGround && ballAbove && horizDist < 250;
				report.AddAvg("Nigel/Ground Dribble Ratio", groundDribble);

				bool airDribble = !player.isOnGround && player.pos.z > 300
					&& state.ball.pos.z > 300 && ballDist < 350;
				report.AddAvg("Nigel/Air Dribble Ratio", airDribble);

				bool aerialPoss = !player.isOnGround && ballDist < 500;
				report.AddAvg("Nigel/Aerial Possession Ratio", aerialPoss);

				bool onWall = player.isOnGround && player.pos.z > 200 &&
					(fabsf(player.pos.x) > 3500 || fabsf(player.pos.y) > 4600);
				report.AddAvg("Nigel/On Wall Ratio", onWall);
			}
		}

		if (state.goalScored) {
			report.AddAvg("Game/Goal Speed", state.ball.vel.Length());
			report.AddAvg("Game/Goal Scored", 1.0f);
		} else {
			report.AddAvg("Game/Goal Scored", 0.0f);
		}
	}
}

int main(int argc, char* argv[]) {
	// Initialize RocketSim (relative to exe path)
	auto exeDir = std::filesystem::path(argv[0]).parent_path();
	RocketSim::Init((exeDir / "collision_meshes").string().c_str());

	QApplication app(argc, argv);
	QQuickStyle::setStyle("Basic");
	app.setWindowIcon(QIcon(QString::fromStdString((exeDir / "nigel.ico").string())));

	// Build default training config
	TrainConfig cfg = {};
	// Use repo-root checkpoints/ so they're git-tracked (3 levels up from exe in out/build/x64-Release/core/)
	auto repoRoot = exeDir.parent_path().parent_path().parent_path().parent_path();
	cfg.checkpointFolder = (repoRoot / "checkpoints").string();
	cfg.deviceType = DeviceType::GPU_CUDA;
	cfg.tickSkip = 8;
	cfg.actionDelay = cfg.tickSkip - 1;
	cfg.numGames = 256;
	cfg.randomSeed = 123;

	int tsPerItr = 150'000;
	cfg.ppo.tsPerItr = tsPerItr;
	cfg.ppo.batchSize = tsPerItr;
	cfg.ppo.miniBatchSize = 50'000;
	cfg.ppo.epochs = 1;
	cfg.ppo.entropyScale = 0.04f;
	cfg.ppo.gaeGamma = 0.997;
	cfg.ppo.gaeLambda = 0.95f;
	cfg.ppo.policyLR = 0.8e-4;
	cfg.ppo.criticLR = 0.8e-4;
	cfg.ppo.useHalfPrecision = true;

	cfg.ppo.sharedHead.layerSizes = { 1024, 1024 };
	cfg.ppo.policy.layerSizes = { 1024, 1024, 1024 };
	cfg.ppo.critic.layerSizes = { 1024, 1024, 1024 };

	auto optim = ModelOptimType::ADAM;
	cfg.ppo.policy.optimType = optim;
	cfg.ppo.critic.optimType = optim;
	cfg.ppo.sharedHead.optimType = optim;

	auto activation = ModelActivationType::LEAKY_RELU;
	cfg.ppo.policy.activationType = activation;
	cfg.ppo.critic.activationType = activation;
	cfg.ppo.sharedHead.activationType = activation;

	cfg.ppo.policy.addLayerNorm = true;
	cfg.ppo.critic.addLayerNorm = true;
	cfg.ppo.sharedHead.addLayerNorm = true;

	cfg.trainAgainstOldVersions = true;
	cfg.trainAgainstOldChance = 0.15f;
	cfg.skillTracker.enabled = false;

	// Create bridge with config and env functions (training starts from UI)
	TrainingBridge bridge;
	bridge.setup(EnvCreateFunc, StepCallback, cfg);

	// Set up QML
	QQmlApplicationEngine engine;
	auto qmlDir = QString::fromStdString((exeDir / "qml").string());
	engine.addImportPath(qmlDir);

	QObject::connect(&engine, &QQmlApplicationEngine::warnings, [](const QList<QQmlError>& warnings) {
		for (const auto& w : warnings)
			fprintf(stderr, "QML Warning: %s\n", w.toString().toUtf8().constData());
	});

	engine.rootContext()->setContextProperty("training", &bridge);

	auto qmlFile = QUrl::fromLocalFile(
		QString::fromStdString((exeDir / "qml" / "Main.qml").string())
	);
	engine.load(qmlFile);

	if (engine.rootObjects().isEmpty()) {
		fprintf(stderr, "ERROR: Failed to load QML from: %s\n", qmlFile.toString().toUtf8().constData());
		fprintf(stderr, "QML import paths:\n");
		for (const auto& p : engine.importPathList())
			fprintf(stderr, "  %s\n", p.toUtf8().constData());
		return 1;
	}

	// Training starts when user clicks Start in the UI
	return app.exec();
}
