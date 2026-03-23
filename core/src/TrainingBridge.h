#pragma once
#include <QObject>
#include <QTimer>
#include <QVariantMap>
#include <QVariantList>
#include <QThread>
#include <QCoreApplication>
#include <QProcess>
#include <deque>
#include "engine/Trainer.h"
#include "util/GPUMonitor.h"

namespace NGL {

	// Runs the trainer on a background thread
	class TrainerWorker : public QObject {
		Q_OBJECT
	public:
		Trainer* trainer = nullptr;
	public Q_SLOTS:
		void run() {
			if (trainer) trainer->Start();
			Q_EMIT finished();
		}
	Q_SIGNALS:
		void finished();
	};

	// Bridge between C++ training and QML UI
	class TrainingBridge : public QObject {
		Q_OBJECT

		// Training state
		Q_PROPERTY(bool isTraining READ isTraining NOTIFY isTrainingChanged)

		// Metrics (updated while training)
		Q_PROPERTY(QString status READ status NOTIFY metricsUpdated)
		Q_PROPERTY(quint64 totalTimesteps READ totalTimesteps NOTIFY metricsUpdated)
		Q_PROPERTY(quint64 totalIterations READ totalIterations NOTIFY metricsUpdated)
		Q_PROPERTY(double overallSPS READ overallSPS NOTIFY metricsUpdated)
		Q_PROPERTY(double collectionSPS READ collectionSPS NOTIFY metricsUpdated)
		Q_PROPERTY(double consumptionSPS READ consumptionSPS NOTIFY metricsUpdated)
		Q_PROPERTY(double avgSPS READ avgSPS NOTIFY metricsUpdated)
		Q_PROPERTY(double avgStepReward READ avgStepReward NOTIFY metricsUpdated)
		Q_PROPERTY(double policyEntropy READ policyEntropy NOTIFY metricsUpdated)
		Q_PROPERTY(double policyLoss READ policyLoss NOTIFY metricsUpdated)
		Q_PROPERTY(double criticLoss READ criticLoss NOTIFY metricsUpdated)
		Q_PROPERTY(double klDivergence READ klDivergence NOTIFY metricsUpdated)
		Q_PROPERTY(double clipFraction READ clipFraction NOTIFY metricsUpdated)
		Q_PROPERTY(double policyUpdateMag READ policyUpdateMag NOTIFY metricsUpdated)
		Q_PROPERTY(double criticUpdateMag READ criticUpdateMag NOTIFY metricsUpdated)
		Q_PROPERTY(double collectionTime READ collectionTime NOTIFY metricsUpdated)
		Q_PROPERTY(double consumptionTime READ consumptionTime NOTIFY metricsUpdated)
		Q_PROPERTY(double gaeTime READ gaeTime NOTIFY metricsUpdated)
		Q_PROPERTY(double ppoLearnTime READ ppoLearnTime NOTIFY metricsUpdated)
		Q_PROPERTY(QVariantList rewardBreakdown READ rewardBreakdown NOTIFY metricsUpdated)
		Q_PROPERTY(QVariantList skillMetrics READ skillMetrics NOTIFY metricsUpdated)
		Q_PROPERTY(QVariantList rewardWeights READ rewardWeights NOTIFY rewardWeightsChanged)

		// GPU
		Q_PROPERTY(int gpuVramUsed READ gpuVramUsed NOTIFY gpuUpdated)
		Q_PROPERTY(int gpuVramTotal READ gpuVramTotal NOTIFY gpuUpdated)
		Q_PROPERTY(int gpuUtilization READ gpuUtilization NOTIFY gpuUpdated)
		Q_PROPERTY(int gpuTemperature READ gpuTemperature NOTIFY gpuUpdated)
		Q_PROPERTY(double gpuPowerDraw READ gpuPowerDraw NOTIFY gpuUpdated)
		Q_PROPERTY(bool gpuAvailable READ gpuAvailable NOTIFY gpuUpdated)
		Q_PROPERTY(QString gpuName READ gpuName NOTIFY gpuUpdated)

		// History for plots
		Q_PROPERTY(QVariantList rewardHistory READ rewardHistory NOTIFY metricsUpdated)
		Q_PROPERTY(QVariantList spsHistory READ spsHistory NOTIFY metricsUpdated)
		Q_PROPERTY(QVariantList entropyHistory READ entropyHistory NOTIFY metricsUpdated)
		Q_PROPERTY(QVariantList policyLossHistory READ policyLossHistory NOTIFY metricsUpdated)
		Q_PROPERTY(QVariantList criticLossHistory READ criticLossHistory NOTIFY metricsUpdated)

		// Config properties (editable before training starts)
		Q_PROPERTY(int cfgNumGames READ cfgNumGames WRITE setCfgNumGames NOTIFY configChanged)
		Q_PROPERTY(int cfgTickSkip READ cfgTickSkip WRITE setCfgTickSkip NOTIFY configChanged)
		Q_PROPERTY(int cfgTsPerItr READ cfgTsPerItr WRITE setCfgTsPerItr NOTIFY configChanged)
		Q_PROPERTY(int cfgBatchSize READ cfgBatchSize WRITE setCfgBatchSize NOTIFY configChanged)
		Q_PROPERTY(int cfgMiniBatchSize READ cfgMiniBatchSize WRITE setCfgMiniBatchSize NOTIFY configChanged)
		Q_PROPERTY(int cfgEpochs READ cfgEpochs WRITE setCfgEpochs NOTIFY configChanged)
		Q_PROPERTY(double cfgEntropyScale READ cfgEntropyScale WRITE setCfgEntropyScale NOTIFY configChanged)
		Q_PROPERTY(double cfgPolicyLR READ cfgPolicyLR WRITE setCfgPolicyLR NOTIFY configChanged)
		Q_PROPERTY(double cfgCriticLR READ cfgCriticLR WRITE setCfgCriticLR NOTIFY configChanged)
		Q_PROPERTY(double cfgGaeGamma READ cfgGaeGamma WRITE setCfgGaeGamma NOTIFY configChanged)
		Q_PROPERTY(double cfgGaeLambda READ cfgGaeLambda WRITE setCfgGaeLambda NOTIFY configChanged)
		Q_PROPERTY(double cfgClipRange READ cfgClipRange WRITE setCfgClipRange NOTIFY configChanged)
		Q_PROPERTY(double cfgPolicyTemp READ cfgPolicyTemp WRITE setCfgPolicyTemp NOTIFY configChanged)
		Q_PROPERTY(double cfgRewardClipRange READ cfgRewardClipRange WRITE setCfgRewardClipRange NOTIFY configChanged)
		Q_PROPERTY(bool cfgTrainAgainstOld READ cfgTrainAgainstOld WRITE setCfgTrainAgainstOld NOTIFY configChanged)
		Q_PROPERTY(double cfgTrainAgainstOldChance READ cfgTrainAgainstOldChance WRITE setCfgTrainAgainstOldChance NOTIFY configChanged)
		Q_PROPERTY(int cfgCheckpointsToKeep READ cfgCheckpointsToKeep WRITE setCfgCheckpointsToKeep NOTIFY configChanged)
		Q_PROPERTY(quint64 autoStopTimesteps READ autoStopTimesteps WRITE setAutoStopTimesteps NOTIFY configChanged)
		Q_PROPERTY(int cfgDeviceType READ cfgDeviceType WRITE setCfgDeviceType NOTIFY configChanged)
		Q_PROPERTY(int cfgRandomSeed READ cfgRandomSeed WRITE setCfgRandomSeed NOTIFY configChanged)
		Q_PROPERTY(bool cfgHalfPrecision READ cfgHalfPrecision WRITE setCfgHalfPrecision NOTIFY configChanged)
		Q_PROPERTY(bool cfgStandardizeObs READ cfgStandardizeObs WRITE setCfgStandardizeObs NOTIFY configChanged)
		Q_PROPERTY(bool cfgStandardizeReturns READ cfgStandardizeReturns WRITE setCfgStandardizeReturns NOTIFY configChanged)

	public:
		explicit TrainingBridge(QObject* parent = nullptr);
		~TrainingBridge();

		// Call before QML loads to set functions and default config
		void setup(EnvCreateFn envCreateFn, StepCallbackFn stepCallbackFn, TrainConfig defaultConfig);

		// Training state
		bool isTraining() const { return m_isTraining; }
		QString status() const;

		// Metrics getters
		quint64 totalTimesteps() const { return m_metrics.totalTimesteps; }
		quint64 totalIterations() const { return m_metrics.totalIterations; }
		double overallSPS() const { return m_metrics.overallSPS; }
		double collectionSPS() const { return m_metrics.collectionSPS; }
		double consumptionSPS() const { return m_metrics.consumptionSPS; }
		double avgSPS() const { return m_avgSPS; }
		double avgStepReward() const { return m_metrics.avgStepReward; }
		double policyEntropy() const { return m_metrics.policyEntropy; }
		double policyLoss() const { return m_metrics.policyLoss; }
		double criticLoss() const { return m_metrics.criticLoss; }
		double klDivergence() const { return m_metrics.klDivergence; }
		double clipFraction() const { return m_metrics.clipFraction; }
		double policyUpdateMag() const { return m_metrics.policyUpdateMag; }
		double criticUpdateMag() const { return m_metrics.criticUpdateMag; }
		double collectionTime() const { return m_metrics.collectionTime; }
		double consumptionTime() const { return m_metrics.consumptionTime; }
		double gaeTime() const { return m_metrics.gaeTime; }
		double ppoLearnTime() const { return m_metrics.ppoLearnTime; }
		QVariantList rewardBreakdown() const;
		QVariantList skillMetrics() const;
		QVariantList rewardWeights() const;

		// GPU getters
		int gpuVramUsed() const { return m_gpu.vramUsedMB; }
		int gpuVramTotal() const { return m_gpu.vramTotalMB; }
		int gpuUtilization() const { return m_gpu.utilization; }
		int gpuTemperature() const { return m_gpu.temperature; }
		double gpuPowerDraw() const { return m_gpu.powerDraw; }
		bool gpuAvailable() const { return m_gpu.available; }
		QString gpuName() const { return QString::fromStdString(m_gpu.gpuName); }

		// History getters
		QVariantList rewardHistory() const { return m_rewardHistory; }
		QVariantList spsHistory() const { return m_spsHistory; }
		QVariantList entropyHistory() const { return m_entropyHistory; }
		QVariantList policyLossHistory() const { return m_policyLossHistory; }
		QVariantList criticLossHistory() const { return m_criticLossHistory; }

		// Config getters
		int cfgNumGames() const { return m_config.numGames; }
		int cfgTickSkip() const { return m_config.tickSkip; }
		int cfgTsPerItr() const { return (int)m_config.ppo.tsPerItr; }
		int cfgBatchSize() const { return (int)m_config.ppo.batchSize; }
		int cfgMiniBatchSize() const { return (int)m_config.ppo.miniBatchSize; }
		int cfgEpochs() const { return m_config.ppo.epochs; }
		double cfgEntropyScale() const { return m_config.ppo.entropyScale; }
		double cfgPolicyLR() const { return m_config.ppo.policyLR; }
		double cfgCriticLR() const { return m_config.ppo.criticLR; }
		double cfgGaeGamma() const { return m_config.ppo.gaeGamma; }
		double cfgGaeLambda() const { return m_config.ppo.gaeLambda; }
		double cfgClipRange() const { return m_config.ppo.clipRange; }
		double cfgPolicyTemp() const { return m_config.ppo.policyTemperature; }
		double cfgRewardClipRange() const { return m_config.ppo.rewardClipRange; }
		bool cfgTrainAgainstOld() const { return m_config.trainAgainstOldVersions; }
		double cfgTrainAgainstOldChance() const { return m_config.trainAgainstOldChance; }
		int cfgCheckpointsToKeep() const { return m_config.checkpointsToKeep; }
		quint64 autoStopTimesteps() const { return m_autoStopTimesteps; }
		int cfgDeviceType() const { return (int)m_config.deviceType; }
		int cfgRandomSeed() const { return (int)m_config.randomSeed; }
		bool cfgHalfPrecision() const { return m_config.ppo.useHalfPrecision; }
		bool cfgStandardizeObs() const { return m_config.standardizeObs; }
		bool cfgStandardizeReturns() const { return m_config.standardizeReturns; }

		// Config setters
		void setCfgNumGames(int v) { m_config.numGames = v; Q_EMIT configChanged(); }
		void setCfgTickSkip(int v) { m_config.tickSkip = v; m_config.actionDelay = v - 1; Q_EMIT configChanged(); }
		void setCfgTsPerItr(int v) { m_config.ppo.tsPerItr = v; Q_EMIT configChanged(); }
		void setCfgBatchSize(int v) { m_config.ppo.batchSize = v; Q_EMIT configChanged(); }
		void setCfgMiniBatchSize(int v) { m_config.ppo.miniBatchSize = v; Q_EMIT configChanged(); }
		void setCfgEpochs(int v) { m_config.ppo.epochs = v; Q_EMIT configChanged(); }
		void setCfgEntropyScale(double v) { m_config.ppo.entropyScale = (float)v; Q_EMIT configChanged(); }
		void setCfgPolicyLR(double v) { m_config.ppo.policyLR = (float)v; Q_EMIT configChanged(); }
		void setCfgCriticLR(double v) { m_config.ppo.criticLR = (float)v; Q_EMIT configChanged(); }
		void setCfgGaeGamma(double v) { m_config.ppo.gaeGamma = (float)v; Q_EMIT configChanged(); }
		void setCfgGaeLambda(double v) { m_config.ppo.gaeLambda = (float)v; Q_EMIT configChanged(); }
		void setCfgClipRange(double v) { m_config.ppo.clipRange = (float)v; Q_EMIT configChanged(); }
		void setCfgPolicyTemp(double v) { m_config.ppo.policyTemperature = (float)v; Q_EMIT configChanged(); }
		void setCfgRewardClipRange(double v) { m_config.ppo.rewardClipRange = (float)v; Q_EMIT configChanged(); }
		void setCfgTrainAgainstOld(bool v) { m_config.trainAgainstOldVersions = v; Q_EMIT configChanged(); }
		void setCfgTrainAgainstOldChance(double v) { m_config.trainAgainstOldChance = (float)v; Q_EMIT configChanged(); }
		void setCfgCheckpointsToKeep(int v) { m_config.checkpointsToKeep = v; Q_EMIT configChanged(); }
		void setAutoStopTimesteps(quint64 v) { m_autoStopTimesteps = v; Q_EMIT configChanged(); }
		void setCfgDeviceType(int v) { m_config.deviceType = (DeviceType)v; Q_EMIT configChanged(); }
		void setCfgRandomSeed(int v) { m_config.randomSeed = v; Q_EMIT configChanged(); }
		void setCfgHalfPrecision(bool v) { m_config.ppo.useHalfPrecision = v; Q_EMIT configChanged(); }
		void setCfgStandardizeObs(bool v) { m_config.standardizeObs = v; Q_EMIT configChanged(); }
		void setCfgStandardizeReturns(bool v) { m_config.standardizeReturns = v; Q_EMIT configChanged(); }

		Q_INVOKABLE QString networkInfo() const;
		Q_INVOKABLE void setRewardWeight(const QString& name, double weight);

		// Console log
		Q_PROPERTY(QVariantList logMessages READ logMessages NOTIFY logAdded)
		QVariantList logMessages() const { return m_logMessages; }
		void log(const QString& message);

	public Q_SLOTS:
		void start();
		void visualize();
		void pause();
		void resume();
		void save();
		void stop();

	Q_SIGNALS:
		void metricsUpdated();
		void gpuUpdated();
		void isTrainingChanged();
		void configChanged();
		void rewardWeightsChanged();
		void logAdded();

	private Q_SLOTS:
		void pollMetrics();
		void pollGPU();
		void onTrainingFinished();

	private:
		Trainer* m_trainer = nullptr;
		GPUMonitor m_gpuMonitor;
		MetricsSnapshot m_metrics;
		GPUStatus m_gpu;
		QTimer* m_metricsTimer = nullptr;
		QTimer* m_gpuTimer = nullptr;

		// Config and functions for deferred Trainer creation
		TrainConfig m_config;
		EnvCreateFn m_envCreateFn;
		StepCallbackFn m_stepCallbackFn;
		QThread* m_trainThread = nullptr;
		QProcess* m_visProcess = nullptr;
		bool m_isTraining = false;

		// Rolling average SPS (last 10 iterations)
		static constexpr int AVG_SPS_WINDOW = 10;
		std::deque<double> m_recentSPS;
		double m_avgSPS = 0;

		static constexpr int MAX_HISTORY = 200;
		QVariantList m_rewardHistory;
		QVariantList m_spsHistory;
		QVariantList m_entropyHistory;
		QVariantList m_policyLossHistory;
		QVariantList m_criticLossHistory;

		// Cached reward weights for display when trainer isn't running
		QVariantList m_cachedRewardWeights;

		// Auto-stop
		quint64 m_autoStopTimesteps = 0;  // 0 = disabled

		// Console log
		static constexpr int MAX_LOG_LINES = 500;
		QVariantList m_logMessages;

		void appendHistory(QVariantList& list, double value);
		void saveConfig();
		void loadConfig();
		void loadCheckpointStats();
		void saveMetrics();
		void loadMetrics();
	};
}
