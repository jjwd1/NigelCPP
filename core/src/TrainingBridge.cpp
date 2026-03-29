#include "TrainingBridge.h"
#include <QFile>
#include <QDebug>
#include <QDateTime>
#include <nlohmann/json.hpp>
#include <fstream>
#include <filesystem>

void NGL::TrainingBridge::log(const QString& message) {
	QString timestamp = QDateTime::currentDateTime().toString("hh:mm:ss");
	QString line = "[" + timestamp + "] " + message;
	m_logMessages.append(line);
	while (m_logMessages.size() > MAX_LOG_LINES)
		m_logMessages.removeFirst();
	qDebug().noquote() << line;
	Q_EMIT logAdded();
}

NGL::TrainingBridge::TrainingBridge(QObject* parent) : QObject(parent) {
	m_metricsTimer = new QTimer(this);
	connect(m_metricsTimer, &QTimer::timeout, this, &TrainingBridge::pollMetrics);

	m_gpuTimer = new QTimer(this);
	connect(m_gpuTimer, &QTimer::timeout, this, &TrainingBridge::pollGPU);
	m_gpuTimer->start(3000);

	connect(this, &TrainingBridge::configChanged, this, &TrainingBridge::saveConfig);
}

NGL::TrainingBridge::~TrainingBridge() {
	if (m_trainer)
		m_trainer->RequestStop();
	if (m_trainThread && m_trainThread->isRunning())
		m_trainThread->wait(5000);
	delete m_trainer;
}

void NGL::TrainingBridge::setup(EnvCreateFn envCreateFn, StepCallbackFn stepCallbackFn, TrainConfig defaultConfig) {
	m_envCreateFn = envCreateFn;
	m_stepCallbackFn = stepCallbackFn;
	m_config = defaultConfig;
	loadConfig();
	loadCheckpointStats();

	log("App started");
	if (m_metrics.totalTimesteps > 0)
		log("Loaded metrics from last session (" + QString::number(m_metrics.totalTimesteps) + " steps)");
	if (!m_cachedRewardWeights.isEmpty())
		log("Loaded " + QString::number(m_cachedRewardWeights.size()) + " saved reward weights");

	// Emit without triggering save (config was just loaded)
	disconnect(this, &TrainingBridge::configChanged, this, &TrainingBridge::saveConfig);
	Q_EMIT configChanged();
	connect(this, &TrainingBridge::configChanged, this, &TrainingBridge::saveConfig);
}

QString NGL::TrainingBridge::status() const {
	if (!m_trainer) return m_isTraining ? "STARTING" : "IDLE";
	switch (m_trainer->GetState()) {
	case TrainerState::INITIALIZING: return "INITIALIZING";
	case TrainerState::RUNNING: return "RUNNING";
	case TrainerState::PAUSED: return "PAUSED";
	case TrainerState::STOPPED: return "STOPPED";
	}
	return "UNKNOWN";
}

QVariantList NGL::TrainingBridge::rewardBreakdown() const {
	// Build a name->weight lookup from trainer or cache
	std::unordered_map<std::string, float> weightMap;
	if (m_trainer) {
		auto weights = const_cast<Trainer*>(m_trainer)->GetRewardWeights();
		for (auto& rw : weights)
			weightMap[rw.name] = rw.weight;
	} else {
		for (const auto& entry : m_cachedRewardWeights) {
			auto map = entry.toMap();
			weightMap[map["name"].toString().toStdString()] = map["weight"].toFloat();
		}
	}

	QVariantList list;
	if (!m_metrics.rewardBreakdown.empty()) {
		for (auto& pair : m_metrics.rewardBreakdown) {
			QVariantMap entry;
			entry["name"] = QString::fromStdString(pair.first);
			entry["value"] = pair.second;
			auto it = weightMap.find(pair.first);
			entry["weight"] = (it != weightMap.end()) ? it->second : -1.0f;
			list.append(entry);
		}
	} else {
		// No values yet — show cached weights so rewards are visible on startup
		for (auto& pair : weightMap) {
			QVariantMap entry;
			entry["name"] = QString::fromStdString(pair.first);
			entry["value"] = 0.0f;
			entry["weight"] = pair.second;
			list.append(entry);
		}
	}

	// Sort by weighted absolute value (most contribution first)
	std::sort(list.begin(), list.end(), [](const QVariant& a, const QVariant& b) {
		auto am = a.toMap();
		auto bm = b.toMap();
		float aw = std::abs(am["value"].toFloat() * am["weight"].toFloat());
		float bw = std::abs(bm["value"].toFloat() * bm["weight"].toFloat());
		return aw > bw;
	});

	return list;
}

QVariantList NGL::TrainingBridge::skillMetrics() const {
	QVariantList list;
	for (auto& pair : m_metrics.skillMetrics) {
		QVariantMap entry;
		entry["name"] = QString::fromStdString(pair.first);
		entry["value"] = pair.second;
		list.append(entry);
	}
	return list;
}

QVariantList NGL::TrainingBridge::rewardWeights() const {
	if (!m_trainer) return m_cachedRewardWeights;
	QVariantList list;
	auto weights = const_cast<Trainer*>(m_trainer)->GetRewardWeights();
	for (auto& rw : weights) {
		QVariantMap entry;
		entry["name"] = QString::fromStdString(rw.name);
		entry["weight"] = rw.weight;
		list.append(entry);
	}
	return list;
}

void NGL::TrainingBridge::setRewardWeight(const QString& name, double weight) {
	// Update trainer if running
	if (m_trainer)
		m_trainer->SetRewardWeight(name.toStdString(), (float)weight);

	// Track as user override so it persists across start/stop
	m_userWeightOverrides[name.toStdString()] = (float)weight;

	// Always update cached weights
	for (int i = 0; i < m_cachedRewardWeights.size(); i++) {
		auto map = m_cachedRewardWeights[i].toMap();
		if (map["name"].toString() == name) {
			map["weight"] = weight;
			m_cachedRewardWeights[i] = map;
			break;
		}
	}

	// Log with live read-back if trainer is running
	if (m_trainer) {
		auto liveWeights = m_trainer->GetRewardWeights();
		for (auto& rw : liveWeights) {
			if (rw.name == name.toStdString()) {
				log(QString::fromStdString(rw.name) + " weight changed to "
					+ QString::number(rw.weight, 'f', 4) + " (verified from trainer)");
				break;
			}
		}
	} else {
		log(name + " weight changed to " + QString::number(weight, 'f', 4) + " (saved, will apply on start)");
	}

	saveMetrics();
	Q_EMIT rewardWeightsChanged();
	Q_EMIT metricsUpdated();
}

QString NGL::TrainingBridge::networkInfo() const {
	auto fmt = [](const std::vector<int>& sizes) -> QString {
		QString s;
		for (size_t i = 0; i < sizes.size(); i++) {
			if (i > 0) s += " -> ";
			s += QString::number(sizes[i]);
		}
		return s;
	};
	return QString("Shared Head: %1\nPolicy: %2\nCritic: %3")
		.arg(fmt(m_config.ppo.sharedHead.layerSizes))
		.arg(fmt(m_config.ppo.policy.layerSizes))
		.arg(fmt(m_config.ppo.critic.layerSizes));
}

void NGL::TrainingBridge::start() {
	if (m_isTraining || m_trainer) return;

	// Clear stale reward values so they repopulate from current code
	m_metrics.rewardBreakdown.clear();

	// Reset session accumulators
	m_rewardBreakdownSum.clear();
	m_rewardBreakdownCount = 0;
	m_spsSumSession = 0;
	m_spsCountSession = 0;
	m_avgSPS = 0;

	m_isTraining = true;
	Q_EMIT isTrainingChanged();
	Q_EMIT metricsUpdated();

	log("Training started (" + QString::number(m_config.numGames) + " games)");

	// Create trainer (blocks briefly during initialization)
	m_trainer = new Trainer(m_envCreateFn, m_config, m_stepCallbackFn);

	// Apply user-edited weight overrides (from UI edits, not the full cache)
	for (auto& pair : m_userWeightOverrides) {
		m_trainer->SetRewardWeight(pair.first, pair.second);
		log("  Applied user override: " + QString::fromStdString(pair.first) + " = " + QString::number(pair.second, 'f', 4));
	}

	// Read back all weights from the live trainer
	auto liveWeights = m_trainer->GetRewardWeights();
	for (auto& rw : liveWeights) {
		log("  " + QString::fromStdString(rw.name) + " = "
			+ QString::number(rw.weight, 'f', 4) + " (live from trainer)");
	}
	Q_EMIT rewardWeightsChanged();

	// Start training on background thread
	m_trainThread = new QThread();
	auto* worker = new TrainerWorker();
	worker->trainer = m_trainer;
	worker->moveToThread(m_trainThread);

	QObject::connect(m_trainThread, &QThread::started, worker, &TrainerWorker::run);
	QObject::connect(worker, &TrainerWorker::finished, this, &TrainingBridge::onTrainingFinished);
	QObject::connect(worker, &TrainerWorker::finished, m_trainThread, &QThread::quit);
	QObject::connect(m_trainThread, &QThread::finished, worker, &QObject::deleteLater);
	QObject::connect(m_trainThread, &QThread::finished, m_trainThread, &QObject::deleteLater);

	m_metricsTimer->start(1000);
	m_trainThread->start();
	Q_EMIT metricsUpdated();
}

void NGL::TrainingBridge::visualize() {
	if (m_isTraining || m_trainer) return;

	// Launch RocketSimVis
	auto visDir = QCoreApplication::applicationDirPath() + "/RocketSimVis";
	auto visScript = visDir + "/src/main.py";

	if (!QFile::exists(visScript)) {
		log("ERROR: Visualizer not found at " + visScript);
		return;
	}

	log("Visualizer started");

	m_visProcess = new QProcess(this);
	m_visProcess->setWorkingDirectory(visDir);
	connect(m_visProcess, &QProcess::finished, this, [this]() {
		log("Visualizer closed");
		if (m_trainer) m_trainer->RequestStop();
	});
	m_visProcess->start("python", QStringList() << visScript);

	// Start trainer in render mode with latest checkpoint
	auto savedRenderMode = m_config.renderMode;
	auto savedNumGames = m_config.numGames;
	m_config.renderMode = true;
	m_config.numGames = 1;

	m_isTraining = true;
	Q_EMIT isTrainingChanged();
	Q_EMIT metricsUpdated();

	m_trainer = new Trainer(m_envCreateFn, m_config, m_stepCallbackFn);

	Q_EMIT rewardWeightsChanged();

	// Restore config for next normal training start
	m_config.renderMode = savedRenderMode;
	m_config.numGames = savedNumGames;

	m_trainThread = new QThread();
	auto* worker = new TrainerWorker();
	worker->trainer = m_trainer;
	worker->moveToThread(m_trainThread);

	QObject::connect(m_trainThread, &QThread::started, worker, &TrainerWorker::run);
	QObject::connect(worker, &TrainerWorker::finished, this, &TrainingBridge::onTrainingFinished);
	QObject::connect(worker, &TrainerWorker::finished, m_trainThread, &QThread::quit);
	QObject::connect(m_trainThread, &QThread::finished, worker, &QObject::deleteLater);
	QObject::connect(m_trainThread, &QThread::finished, m_trainThread, &QObject::deleteLater);

	m_metricsTimer->start(1000);
	m_trainThread->start();
	Q_EMIT metricsUpdated();
}

void NGL::TrainingBridge::pause() {
	if (m_trainer) { m_trainer->RequestPause(); log("Training paused"); Q_EMIT metricsUpdated(); }
}

void NGL::TrainingBridge::resume() {
	if (m_trainer) { m_trainer->RequestResume(); log("Training resumed"); Q_EMIT metricsUpdated(); }
}

void NGL::TrainingBridge::save() {
	if (m_trainer) { m_trainer->RequestSave(); log("Checkpoint save requested"); }
}

void NGL::TrainingBridge::stop() {
	if (m_trainer) { m_trainer->RequestStop(); log("Training stop requested"); }
}

void NGL::TrainingBridge::onTrainingFinished() {
	m_metricsTimer->stop();
	m_isTraining = false;
	m_avgSPS = 0;
	if (m_visProcess) {
		m_visProcess->terminate();
		m_visProcess->deleteLater();
		m_visProcess = nullptr;
	}
	log("Training stopped at " + QString::number(m_metrics.totalTimesteps) + " steps");

	// Cache reward weights before destroying trainer so they persist in the UI
	m_cachedRewardWeights.clear();
	auto weights = m_trainer->GetRewardWeights();
	for (auto& rw : weights) {
		QVariantMap entry;
		entry["name"] = QString::fromStdString(rw.name);
		entry["weight"] = rw.weight;
		m_cachedRewardWeights.append(entry);
	}

	delete m_trainer;
	m_trainer = nullptr;
	m_trainThread = nullptr;
	saveMetrics();
	Q_EMIT isTrainingChanged();
	Q_EMIT metricsUpdated();
}

void NGL::TrainingBridge::pollMetrics() {
	if (!m_trainer) return;

	// Check for checkpoint events from training thread
	int64_t saved = m_trainer->lastSavedTimesteps.exchange(-1);
	if (saved >= 0)
		log("Checkpoint saved at " + QString::number(saved) + " steps");

	int64_t loaded = m_trainer->lastLoadedTimesteps.exchange(-1);
	if (loaded >= 0)
		log("Checkpoint loaded from " + QString::number(loaded));

	auto newMetrics = m_trainer->GetLatestMetrics();
	if (newMetrics.totalIterations == m_metrics.totalIterations) return;

	// Accumulate reward breakdown running averages before overwriting
	if (!newMetrics.rewardBreakdown.empty()) {
		m_rewardBreakdownCount++;
		for (auto& pair : newMetrics.rewardBreakdown)
			m_rewardBreakdownSum[pair.first] += pair.second;
	}

	m_metrics = newMetrics;

	// Replace per-iteration values with session averages
	for (auto& pair : m_rewardBreakdownSum)
		m_metrics.rewardBreakdown[pair.first] = (float)(pair.second / m_rewardBreakdownCount);

	// Session average SPS
	m_spsSumSession += m_metrics.overallSPS;
	m_spsCountSession++;
	m_avgSPS = m_spsSumSession / m_spsCountSession;

	appendHistory(m_rewardHistory, m_metrics.avgStepReward);
	appendHistory(m_spsHistory, m_metrics.overallSPS);
	appendHistory(m_entropyHistory, m_metrics.policyEntropy);
	appendHistory(m_policyLossHistory, m_metrics.policyLoss);
	appendHistory(m_criticLossHistory, m_metrics.criticLoss);

	saveMetrics();
	Q_EMIT metricsUpdated();

	// Auto-stop check
	if (m_autoStopTimesteps > 0 && m_metrics.totalTimesteps >= m_autoStopTimesteps) {
		log("Auto-stop triggered at " + QString::number(m_metrics.totalTimesteps) + " steps (target: " + QString::number(m_autoStopTimesteps) + ")");
		save();
		stop();
	}
}

void NGL::TrainingBridge::pollGPU() {
	m_gpu = m_gpuMonitor.GetStatus();
	Q_EMIT gpuUpdated();
}

void NGL::TrainingBridge::appendHistory(QVariantList& list, double value) {
	list.append(value);
	while (list.size() > MAX_HISTORY)
		list.removeFirst();
}

void NGL::TrainingBridge::saveConfig() {
	if (m_isTraining) return; // Don't save while training

	auto path = QCoreApplication::applicationDirPath().toStdString() + "/config.json";
	nlohmann::json j;

	j["numGames"] = m_config.numGames;
	j["tickSkip"] = m_config.tickSkip;
	j["deviceType"] = (int)m_config.deviceType;
	j["randomSeed"] = m_config.randomSeed;

	j["tsPerItr"] = m_config.ppo.tsPerItr;
	j["batchSize"] = m_config.ppo.batchSize;
	j["miniBatchSize"] = m_config.ppo.miniBatchSize;
	j["epochs"] = m_config.ppo.epochs;
	j["clipRange"] = m_config.ppo.clipRange;
	j["policyTemperature"] = m_config.ppo.policyTemperature;

	j["policyLR"] = m_config.ppo.policyLR;
	j["criticLR"] = m_config.ppo.criticLR;

	j["entropyScale"] = m_config.ppo.entropyScale;
	j["gaeGamma"] = m_config.ppo.gaeGamma;
	j["gaeLambda"] = m_config.ppo.gaeLambda;
	j["rewardClipRange"] = m_config.ppo.rewardClipRange;

	j["trainAgainstOldVersions"] = m_config.trainAgainstOldVersions;
	j["trainAgainstOldChance"] = m_config.trainAgainstOldChance;
	j["checkpointsToKeep"] = m_config.checkpointsToKeep;

	j["useHalfPrecision"] = m_config.ppo.useHalfPrecision;
	j["standardizeObs"] = m_config.standardizeObs;
	j["standardizeReturns"] = m_config.standardizeReturns;

	std::ofstream f(path);
	if (f.good())
		f << j.dump(4);
}

void NGL::TrainingBridge::loadConfig() {
	auto path = QCoreApplication::applicationDirPath().toStdString() + "/config.json";
	std::ifstream f(path);
	if (!f.good()) return; // No saved config, use defaults

	try {
		nlohmann::json j = nlohmann::json::parse(f);

		if (j.contains("numGames")) m_config.numGames = j["numGames"];
		if (j.contains("tickSkip")) { m_config.tickSkip = j["tickSkip"]; m_config.actionDelay = m_config.tickSkip - 1; }
		if (j.contains("deviceType")) m_config.deviceType = (DeviceType)(int)j["deviceType"];
		if (j.contains("randomSeed")) m_config.randomSeed = j["randomSeed"];

		if (j.contains("tsPerItr")) m_config.ppo.tsPerItr = j["tsPerItr"];
		if (j.contains("batchSize")) m_config.ppo.batchSize = j["batchSize"];
		if (j.contains("miniBatchSize")) m_config.ppo.miniBatchSize = j["miniBatchSize"];
		if (j.contains("epochs")) m_config.ppo.epochs = j["epochs"];
		if (j.contains("clipRange")) m_config.ppo.clipRange = j["clipRange"];
		if (j.contains("policyTemperature")) m_config.ppo.policyTemperature = j["policyTemperature"];

		if (j.contains("policyLR")) m_config.ppo.policyLR = j["policyLR"];
		if (j.contains("criticLR")) m_config.ppo.criticLR = j["criticLR"];

		if (j.contains("entropyScale")) m_config.ppo.entropyScale = j["entropyScale"];
		if (j.contains("gaeGamma")) m_config.ppo.gaeGamma = j["gaeGamma"];
		if (j.contains("gaeLambda")) m_config.ppo.gaeLambda = j["gaeLambda"];
		if (j.contains("rewardClipRange")) m_config.ppo.rewardClipRange = j["rewardClipRange"];

		if (j.contains("trainAgainstOldVersions")) m_config.trainAgainstOldVersions = j["trainAgainstOldVersions"];
		if (j.contains("trainAgainstOldChance")) m_config.trainAgainstOldChance = j["trainAgainstOldChance"];
		if (j.contains("checkpointsToKeep")) m_config.checkpointsToKeep = j["checkpointsToKeep"];

		if (j.contains("useHalfPrecision")) m_config.ppo.useHalfPrecision = j["useHalfPrecision"];
		if (j.contains("standardizeObs")) m_config.standardizeObs = j["standardizeObs"];
		if (j.contains("standardizeReturns")) m_config.standardizeReturns = j["standardizeReturns"];
	} catch (...) {
		// Corrupt config file, ignore and use defaults
	}
}

void NGL::TrainingBridge::loadCheckpointStats() {
	// Load saved metrics snapshot (rewards, losses, etc.)
	loadMetrics();

	// Load timesteps/iterations from latest checkpoint (overrides metrics file if newer)
	auto checkpointDir = QCoreApplication::applicationDirPath().toStdString() + "/checkpoints";
	if (std::filesystem::exists(checkpointDir)) {
		int64_t highest = -1;
		try {
			for (auto& entry : std::filesystem::directory_iterator(checkpointDir)) {
				if (!entry.is_directory()) continue;
				try {
					int64_t ts = std::stoll(entry.path().filename().string());
					if (ts > highest) highest = ts;
				} catch (...) {}
			}
		} catch (...) {}

		if (highest != -1) {
			auto statsPath = std::filesystem::path(checkpointDir) / std::to_string(highest) / "RUNNING_STATS.json";
			std::ifstream f(statsPath);
			if (f.good()) {
				try {
					nlohmann::json j = nlohmann::json::parse(f);
					if (j.contains("total_timesteps")) m_metrics.totalTimesteps = j["total_timesteps"];
					if (j.contains("total_iterations")) m_metrics.totalIterations = j["total_iterations"];
				} catch (...) {}
			}
		}
	}

	Q_EMIT metricsUpdated();
	Q_EMIT rewardWeightsChanged();
}

void NGL::TrainingBridge::saveMetrics() {
	auto path = QCoreApplication::applicationDirPath().toStdString() + "/metrics.json";
	nlohmann::json j;

	j["totalTimesteps"] = m_metrics.totalTimesteps;
	j["totalIterations"] = m_metrics.totalIterations;
	j["overallSPS"] = m_metrics.overallSPS;
	j["collectionSPS"] = m_metrics.collectionSPS;
	j["consumptionSPS"] = m_metrics.consumptionSPS;
	j["avgStepReward"] = m_metrics.avgStepReward;
	j["policyEntropy"] = m_metrics.policyEntropy;
	j["policyLoss"] = m_metrics.policyLoss;
	j["criticLoss"] = m_metrics.criticLoss;
	j["klDivergence"] = m_metrics.klDivergence;
	j["clipFraction"] = m_metrics.clipFraction;
	j["policyUpdateMag"] = m_metrics.policyUpdateMag;
	j["criticUpdateMag"] = m_metrics.criticUpdateMag;
	j["collectionTime"] = m_metrics.collectionTime;
	j["consumptionTime"] = m_metrics.consumptionTime;
	j["gaeTime"] = m_metrics.gaeTime;
	j["ppoLearnTime"] = m_metrics.ppoLearnTime;

	nlohmann::json rewards = nlohmann::json::object();
	for (auto& pair : m_metrics.rewardBreakdown)
		rewards[pair.first] = pair.second;
	j["rewardBreakdown"] = rewards;

	nlohmann::json skills = nlohmann::json::object();
	for (auto& pair : m_metrics.skillMetrics)
		skills[pair.first] = pair.second;
	j["skillMetrics"] = skills;

	// Save reward weights
	if (m_trainer) {
		nlohmann::json weights = nlohmann::json::object();
		for (auto& rw : m_trainer->GetRewardWeights())
			weights[rw.name] = rw.weight;
		j["rewardWeights"] = weights;
	} else if (!m_cachedRewardWeights.isEmpty()) {
		nlohmann::json weights = nlohmann::json::object();
		for (const auto& entry : m_cachedRewardWeights) {
			auto map = entry.toMap();
			weights[map["name"].toString().toStdString()] = map["weight"].toFloat();
		}
		j["rewardWeights"] = weights;
	}

	// Save user weight overrides
	if (!m_userWeightOverrides.empty()) {
		nlohmann::json overrides = nlohmann::json::object();
		for (auto& pair : m_userWeightOverrides)
			overrides[pair.first] = pair.second;
		j["userWeightOverrides"] = overrides;
	}

	std::ofstream f(path);
	if (f.good()) f << j.dump(4);
}

void NGL::TrainingBridge::loadMetrics() {
	auto path = QCoreApplication::applicationDirPath().toStdString() + "/metrics.json";
	std::ifstream f(path);
	if (!f.good()) return;

	try {
		nlohmann::json j = nlohmann::json::parse(f);

		if (j.contains("totalTimesteps")) m_metrics.totalTimesteps = j["totalTimesteps"];
		if (j.contains("totalIterations")) m_metrics.totalIterations = j["totalIterations"];
		if (j.contains("overallSPS")) m_metrics.overallSPS = j["overallSPS"];
		if (j.contains("collectionSPS")) m_metrics.collectionSPS = j["collectionSPS"];
		if (j.contains("consumptionSPS")) m_metrics.consumptionSPS = j["consumptionSPS"];
		if (j.contains("avgStepReward")) m_metrics.avgStepReward = j["avgStepReward"];
		if (j.contains("policyEntropy")) m_metrics.policyEntropy = j["policyEntropy"];
		if (j.contains("policyLoss")) m_metrics.policyLoss = j["policyLoss"];
		if (j.contains("criticLoss")) m_metrics.criticLoss = j["criticLoss"];
		if (j.contains("klDivergence")) m_metrics.klDivergence = j["klDivergence"];
		if (j.contains("clipFraction")) m_metrics.clipFraction = j["clipFraction"];
		if (j.contains("policyUpdateMag")) m_metrics.policyUpdateMag = j["policyUpdateMag"];
		if (j.contains("criticUpdateMag")) m_metrics.criticUpdateMag = j["criticUpdateMag"];
		if (j.contains("collectionTime")) m_metrics.collectionTime = j["collectionTime"];
		if (j.contains("consumptionTime")) m_metrics.consumptionTime = j["consumptionTime"];
		if (j.contains("gaeTime")) m_metrics.gaeTime = j["gaeTime"];
		if (j.contains("ppoLearnTime")) m_metrics.ppoLearnTime = j["ppoLearnTime"];

		if (j.contains("rewardBreakdown")) {
			for (auto& [key, val] : j["rewardBreakdown"].items())
				m_metrics.rewardBreakdown[key] = val.get<float>();
		}
		if (j.contains("skillMetrics")) {
			for (auto& [key, val] : j["skillMetrics"].items())
				m_metrics.skillMetrics[key] = val.get<float>();
		}
		if (j.contains("rewardWeights")) {
			m_cachedRewardWeights.clear();
			for (auto& [key, val] : j["rewardWeights"].items()) {
				QVariantMap entry;
				entry["name"] = QString::fromStdString(key);
				entry["weight"] = val.get<float>();
				m_cachedRewardWeights.append(entry);
			}
		}
		if (j.contains("userWeightOverrides")) {
			m_userWeightOverrides.clear();
			for (auto& [key, val] : j["userWeightOverrides"].items())
				m_userWeightOverrides[key] = val.get<float>();
		}
	} catch (...) {}
}
