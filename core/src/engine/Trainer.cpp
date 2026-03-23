#include "Trainer.h"
#include "../ppo/GAE.h"
#include "../util/AvgTracker.h"

#include <torch/cuda.h>
#include <nlohmann/json.hpp>

#ifdef RG_CUDA_SUPPORT
#include <c10/cuda/CUDACachingAllocator.h>
#endif

using namespace RLGC;

constexpr const char* STATS_FILE_NAME = "RUNNING_STATS.json";

NGL::Trainer::Trainer(EnvCreateFn envCreateFn, TrainConfig config, StepCallbackFn stepCallback) :
	config(config), stepCallback(stepCallback) {

#ifndef NDEBUG
	RG_LOG("===========================");
	RG_LOG("WARNING: NigelCPP runs extremely slowly in debug mode.");
	RG_LOG("Compile in release mode for training.");
	RG_SLEEP(1000);
#endif

	if (config.tsPerSave == 0)
		config.tsPerSave = config.ppo.tsPerItr;

	RG_LOG("Trainer::Trainer():");

	if (config.randomSeed == -1)
		config.randomSeed = RS_CUR_MS();

	RG_LOG("\tCheckpoint Dir: " << config.checkpointFolder);
	torch::manual_seed(config.randomSeed);

	// Device selection
	at::Device device = at::Device(at::kCPU);
	if (config.deviceType == DeviceType::GPU_CUDA ||
		(config.deviceType == DeviceType::AUTO && torch::cuda::is_available())) {
		RG_LOG("\tUsing CUDA GPU device...");

		torch::Tensor t;
		bool deviceTestFailed = false;
		try {
			t = torch::tensor(0);
			t = t.to(at::Device(at::kCUDA));
			t = t.cpu();
		} catch (...) {
			deviceTestFailed = true;
		}

		if (!torch::cuda::is_available() || deviceTestFailed)
			RG_ERR_CLOSE(
				"Trainer: Can't use CUDA GPU because " <<
				(torch::cuda::is_available() ? "libtorch cannot access the GPU" : "CUDA is not available") << ".\n" <<
				"Make sure your libtorch comes with CUDA support, and that CUDA is installed properly."
			)
		device = at::Device(at::kCUDA);

		// Enable TF32 for tensor core matmul (Ampere+ GPUs, ~2x faster, negligible precision loss)
		at::globalContext().setAllowTF32CuBLAS(true);
		at::globalContext().setAllowTF32CuDNN(true);
		RG_LOG("\tTF32 enabled for cuBLAS and cuDNN");

		// Let cuDNN auto-tune convolution algorithms for the input sizes we use
		at::globalContext().setBenchmarkCuDNN(true);
		RG_LOG("\tcuDNN benchmark mode enabled");
	} else {
		RG_LOG("\tUsing CPU device...");
	}

	// RocketSim
	if (RocketSim::GetStage() != RocketSimStage::INITIALIZED) {
		RG_LOG("\tInitializing RocketSim...");
		RocketSim::Init("collision_meshes", true);
	}

	// Environments
	{
		RG_LOG("\tCreating envs...");
		EnvSetConfig envSetConfig = {};
		envSetConfig.envCreateFn = envCreateFn;
		envSetConfig.numArenas = config.renderMode ? 1 : config.numGames;
		envSetConfig.tickSkip = config.tickSkip;
		envSetConfig.actionDelay = config.actionDelay;
		envSetConfig.saveRewards = config.addRewardsToMetrics;
		envSet = new RLGC::EnvSet(envSetConfig);
		obsSize = envSet->state.obs.size[1];
		numActions = envSet->actionParsers[0]->GetActionAmount();
	}

	// Statistics
	returnStat = config.standardizeReturns ? new WelfordStat() : nullptr;
	obsStat = config.standardizeObs ? new BatchedWelfordStat(obsSize) : nullptr;

	// PPO
	try {
		RG_LOG("\tMaking PPO learner...");
		ppo = new PPOLearner(obsSize, numActions, config.ppo, device);
	} catch (std::exception& e) {
		RG_ERR_CLOSE("Failed to create PPO learner: " << e.what());
	}

	// Render sender (visualization mode)
	if (config.renderMode)
		renderSender = new RenderSender(config.renderTimeScale);

	// Policy versions
	if (config.trainAgainstOldVersions || config.skillTracker.enabled)
		config.savePolicyVersions = true;

	if (config.savePolicyVersions && !config.renderMode) {
		if (config.checkpointFolder.empty())
			RG_ERR_CLOSE("Cannot save/load old policy versions with no checkpoint save folder");
		versionMgr = new PolicyVersionManager(
			config.checkpointFolder / "policy_versions", config.maxOldVersions, config.tsPerVersion,
			config.skillTracker, envSet->config
		);
	}

	// Load checkpoint
	if (!config.checkpointFolder.empty())
		Load();

	if (config.savePolicyVersions && !config.renderMode) {
		auto models = ppo->GetPolicyModels();
		versionMgr->LoadVersions(models, totalTimesteps);
	}

	state = TrainerState::RUNNING;
	RG_LOG(RG_DIVIDER);
}

void NGL::Trainer::Start() {
	RG_LOG("Trainer::Start():");
	RG_LOG("\tObs size: " << obsSize);
	RG_LOG("\tAction amount: " << numActions);

	try {
		ExperienceBuffer experience = ExperienceBuffer(config.randomSeed, torch::kCPU);
		int numPlayers = envSet->state.numPlayers;

		struct Trajectory {
			FList states, nextStates, rewards, logProbs;
			std::vector<uint8_t> actionMasks;
			std::vector<int8_t> terminals;
			std::vector<int32_t> actions;
			void Clear() { *this = Trajectory(); }
			void Append(const Trajectory& other) {
				states += other.states; nextStates += other.nextStates;
				rewards += other.rewards; logProbs += other.logProbs;
				actionMasks += other.actionMasks; terminals += other.terminals;
				actions += other.actions;
			}
			size_t Length() const { return actions.size(); }
		};

		auto trajectories = std::vector<Trajectory>(numPlayers, Trajectory{});
		int maxEpisodeLength = (int)(config.ppo.maxEpisodeDuration * (120.f / config.tickSkip));

		while (!stopRequested) {
			// Handle pause
			while (state == TrainerState::PAUSED && !stopRequested) {
				RG_SLEEP(100);
			}
			if (stopRequested) break;

			// Handle save request
			if (saveRequested) {
				saveRequested = false;
				if (!config.checkpointFolder.empty()) Save();
			}

			Report report = {};
			bool isFirstIteration = (totalTimesteps == 0);

			// Old version setup
			PolicyVersion* oldVersion = nullptr;
			std::vector<bool> oldVersionPlayerMask;
			std::vector<int> newPlayerIndices = {}, oldPlayerIndices = {};
			torch::Tensor tNewPlayerIndices, tOldPlayerIndices;

			for (int i = 0; i < numPlayers; i++)
				newPlayerIndices.push_back(i);

			if (config.trainAgainstOldVersions) {
				bool shouldTrainAgainstOld =
					(RocketSim::Math::RandFloat() < config.trainAgainstOldChance)
					&& !versionMgr->versions.empty();

				if (shouldTrainAgainstOld) {
					int oldVersionIdx = RocketSim::Math::RandInt(0, versionMgr->versions.size());
					oldVersion = &versionMgr->versions[oldVersionIdx];
					Team oldVersionTeam = Team(RocketSim::Math::RandInt(0, 2));

					newPlayerIndices.clear();
					oldVersionPlayerMask.resize(numPlayers);
					int i = 0;
					for (auto& gs : envSet->state.gameStates) {
						for (auto& player : gs.players) {
							if (player.team == oldVersionTeam) {
								oldVersionPlayerMask[i] = true;
								oldPlayerIndices.push_back(i);
							} else {
								oldVersionPlayerMask[i] = false;
								newPlayerIndices.push_back(i);
							}
							i++;
						}
					}
					tNewPlayerIndices = torch::tensor(newPlayerIndices);
					tOldPlayerIndices = torch::tensor(oldPlayerIndices);
				}
			}

			int numRealPlayers = oldVersion ? newPlayerIndices.size() : envSet->state.numPlayers;
			int stepsCollected = 0;

			{ // Generate experience
				auto combinedTraj = Trajectory();
				Timer collectionTimer = {};

				{ // Collect timesteps
					RG_NO_GRAD;
					float inferTime = 0, envStepTime = 0;

					for (int step = 0; combinedTraj.Length() < config.ppo.tsPerItr; step++, stepsCollected += numRealPlayers) {
						Timer stepTimer = {};
						envSet->Reset();
						envStepTime += stepTimer.Elapsed();

						for (float f : envSet->state.obs.data)
							if (isnan(f) || isinf(f))
								RG_ERR_CLOSE("Obs builder produced a NaN/inf value");

						if (obsStat) {
							int numSamples = RS_MIN(envSet->state.numPlayers, config.maxObsSamples);
							for (int i = 0; i < numSamples; i++) {
								int idx = RocketSim::Math::RandInt(0, envSet->state.numPlayers);
								obsStat->IncrementRow(&envSet->state.obs.At(idx, 0));
							}
							auto mean = obsStat->GetMean();
							auto std = obsStat->GetSTD();
							for (double& f : mean) f = RS_CLAMP(f, -config.maxObsMeanRange, config.maxObsMeanRange);
							for (double& f : std) f = RS_MAX(f, config.minObsSTD);
							for (int i = 0; i < envSet->state.numPlayers; i++)
								for (int j = 0; j < obsSize; j++) {
									float& obsVal = envSet->state.obs.At(i, j);
									obsVal = (obsVal - mean[j]) / std[j];
								}
						}

						torch::Tensor tActions, tLogProbs;
						bool useGPU = ppo->device.is_cuda();
						torch::Tensor tStates = useGPU
							? DIMLIST2_TO_TENSOR_PINNED<float>(envSet->state.obs)
							: DIMLIST2_TO_TENSOR<float>(envSet->state.obs);
						torch::Tensor tActionMasks = useGPU
							? DIMLIST2_TO_TENSOR_PINNED<uint8_t>(envSet->state.actionMasks)
							: DIMLIST2_TO_TENSOR<uint8_t>(envSet->state.actionMasks);

						for (int newPlayerIdx : newPlayerIndices) {
							trajectories[newPlayerIdx].states += envSet->state.obs.GetRow(newPlayerIdx);
							trajectories[newPlayerIdx].actionMasks += envSet->state.actionMasks.GetRow(newPlayerIdx);
						}

						envSet->StepFirstHalf(true);

						Timer inferTimer = {};
						if (oldVersion) {
							torch::Tensor tdNewStates = tStates.index_select(0, tNewPlayerIndices).to(ppo->device, true);
							torch::Tensor tdOldStates = tStates.index_select(0, tOldPlayerIndices).to(ppo->device, true);
							torch::Tensor tdNewActionMasks = tActionMasks.index_select(0, tNewPlayerIndices).to(ppo->device, true);
							torch::Tensor tdOldActionMasks = tActionMasks.index_select(0, tOldPlayerIndices).to(ppo->device, true);

							torch::Tensor tNewActions, tOldActions;
							ppo->InferActions(tdNewStates, tdNewActionMasks, &tNewActions, &tLogProbs);
							ppo->InferActions(tdOldStates, tdOldActionMasks, &tOldActions, nullptr, &oldVersion->models);

							tActions = torch::zeros(numPlayers, tNewActions.dtype());
							tActions.index_copy_(0, tNewPlayerIndices, tNewActions.cpu());
							tActions.index_copy_(0, tOldPlayerIndices, tOldActions.cpu());
						} else {
							torch::Tensor tdStates = tStates.to(ppo->device, true);
							torch::Tensor tdActionMasks = tActionMasks.to(ppo->device, true);
							ppo->InferActions(tdStates, tdActionMasks, &tActions, &tLogProbs);
							tActions = tActions.cpu();
						}
						inferTime += inferTimer.Elapsed();

						auto curActions = TENSOR_TO_VEC<int>(tActions);
						FList newLogProbs;
						if (tLogProbs.defined())
							newLogProbs = TENSOR_TO_VEC<float>(tLogProbs);

						stepTimer.Reset();
						envSet->Sync();
						envSet->StepSecondHalf(curActions, false);
						envStepTime += stepTimer.Elapsed();

						if (stepCallback)
							stepCallback(this, envSet->state.gameStates, report);

						if (renderSender) {
							renderSender->Send(envSet->state.gameStates[0]);
							if (stopRequested) break;
							continue;
						}

						// Reward metrics
						if (config.addRewardsToMetrics && (RocketSim::Math::RandInt(0, config.rewardSampleRandInterval) == 0)) {
							int numSamples = RS_MIN((int)envSet->arenas.size(), config.maxRewardSamples);
							std::unordered_map<std::string, AvgTracker> avgRewards = {};
							for (int i = 0; i < numSamples; i++) {
								int arenaIdx = RocketSim::Math::RandInt(0, envSet->arenas.size());
								auto& prevRewards = envSet->state.lastRewards[arenaIdx];
								for (int j = 0; j < envSet->rewards[arenaIdx].size(); j++) {
									std::string rewardName = envSet->rewards[arenaIdx][j].reward->GetName();
									avgRewards[rewardName] += prevRewards[j];
								}
							}
							for (auto& pair : avgRewards)
								report.AddAvg("Rewards/" + pair.first, pair.second.Get());
						}

						// Add to trajectories
						int i = 0;
						for (int newPlayerIdx : newPlayerIndices) {
							trajectories[newPlayerIdx].actions.push_back(curActions[newPlayerIdx]);
							trajectories[newPlayerIdx].rewards += envSet->state.rewards[newPlayerIdx];
							trajectories[newPlayerIdx].logProbs += newLogProbs[i];
							i++;
						}

						// Check terminals
						auto curTerminals = std::vector<uint8_t>(numPlayers, 0);
						for (int idx = 0; idx < envSet->arenas.size(); idx++) {
							uint8_t terminalType = envSet->state.terminals[idx];
							if (!terminalType) continue;
							auto playerStartIdx = envSet->state.arenaPlayerStartIdx[idx];
							int playersInArena = envSet->state.gameStates[idx].players.size();
							for (int i = 0; i < playersInArena; i++)
								curTerminals[playerStartIdx + i] = terminalType;
						}

						for (int newPlayerIdx : newPlayerIndices) {
							int8_t terminalType = curTerminals[newPlayerIdx];
							auto& traj = trajectories[newPlayerIdx];

							if (!terminalType && traj.Length() >= maxEpisodeLength)
								terminalType = RLGC::TerminalType::TRUNCATED;

							traj.terminals.push_back(terminalType);
							if (terminalType) {
								if (terminalType == RLGC::TerminalType::TRUNCATED)
									traj.nextStates += envSet->state.obs.GetRow(newPlayerIdx);
								combinedTraj.Append(traj);
								traj.Clear();
							}
						}
					}

					report["Inference Time"] = inferTime;
					report["Env Step Time"] = envStepTime;
				}
				float collectionTime = collectionTimer.Elapsed();

				Timer consumptionTimer = {};
				{ // Process timesteps
					RG_NO_GRAD;
					bool gpuConsume = ppo->device.is_cuda();

					auto mkTensor = [gpuConsume](auto& vec, torch::ScalarType dtype) {
						if (gpuConsume) {
							auto opts = torch::TensorOptions().dtype(dtype).pinned_memory(true);
							auto t = torch::empty({ (int64_t)vec.size() }, opts);
							memcpy(t.data_ptr(), vec.data(), vec.size() * t.element_size());
							return t;
						}
						return torch::tensor(vec);
					};

					torch::Tensor tStates = mkTensor(combinedTraj.states, torch::kFloat32).reshape({ -1, obsSize });
					torch::Tensor tActionMasks = mkTensor(combinedTraj.actionMasks, torch::kUInt8).reshape({ -1, numActions });
					torch::Tensor tActions = mkTensor(combinedTraj.actions, torch::kInt32);
					torch::Tensor tLogProbs = mkTensor(combinedTraj.logProbs, torch::kFloat32);
					torch::Tensor tRewards = mkTensor(combinedTraj.rewards, torch::kFloat32);
					torch::Tensor tTerminals = mkTensor(combinedTraj.terminals, torch::kInt8);

					torch::Tensor tNextTruncStates;
					if (!combinedTraj.nextStates.empty())
						tNextTruncStates = mkTensor(combinedTraj.nextStates, torch::kFloat32).reshape({ -1, obsSize });

					report["Average Step Reward"] = tRewards.mean().item<float>();
					report["Collected Timesteps"] = stepsCollected;

					torch::Tensor tValPreds, tTruncValPreds;
					if (ppo->device.is_cpu()) {
						tValPreds = ppo->InferCritic(tStates.to(ppo->device, true, true)).cpu();
						if (tNextTruncStates.defined())
							tTruncValPreds = ppo->InferCritic(tNextTruncStates.to(ppo->device, true, true)).cpu();
					} else {
						tValPreds = torch::zeros({ (int64_t)combinedTraj.Length() });
						for (int i = 0; i < combinedTraj.Length(); i += ppo->config.miniBatchSize) {
							int start = i;
							int end = RS_MIN(i + ppo->config.miniBatchSize, (int)combinedTraj.Length());
							auto valPredsPart = ppo->InferCritic(tStates.slice(0, start, end).to(ppo->device, true, true)).cpu();
							tValPreds.slice(0, start, end).copy_(valPredsPart, true);
						}
						if (tNextTruncStates.defined()) {
							RG_ASSERT(tNextTruncStates.size(0) <= ppo->config.miniBatchSize);
							tTruncValPreds = ppo->InferCritic(tNextTruncStates.to(ppo->device, true, true)).cpu();
						}
					}

					report["Episode Length"] = 1.f / (tTerminals == 1).to(torch::kFloat32).mean().item<float>();

					Timer gaeTimer = {};
					torch::Tensor tAdvantages, tTargetVals, tReturns;
					float rewClipPortion;
					GAE::Compute(
						tRewards, tTerminals, tValPreds, tTruncValPreds,
						tAdvantages, tTargetVals, tReturns, rewClipPortion,
						config.ppo.gaeGamma, config.ppo.gaeLambda, returnStat ? returnStat->GetSTD() : 1, config.ppo.rewardClipRange
					);
					report["GAE Time"] = gaeTimer.Elapsed();
					report["Clipped Reward Portion"] = rewClipPortion;

					if (returnStat) {
						report["GAE/Returns STD"] = returnStat->GetSTD();
						int numToIncrement = RS_MIN(config.maxReturnSamples, (int)tReturns.size(0));
						if (numToIncrement > 0) {
							auto selectedReturns = tReturns.index_select(0, torch::randint(tReturns.size(0), { (int64_t)numToIncrement }));
							returnStat->Increment(TENSOR_TO_VEC<float>(selectedReturns));
						}
					}
					report["GAE/Avg Return"] = tReturns.abs().mean().item<float>();
					report["GAE/Avg Advantage"] = tAdvantages.abs().mean().item<float>();
					report["GAE/Avg Val Target"] = tTargetVals.abs().mean().item<float>();

					experience.data.actions = tActions;
					experience.data.logProbs = tLogProbs;
					experience.data.actionMasks = tActionMasks;
					experience.data.states = tStates;
					experience.data.advantages = tAdvantages;
					experience.data.targetValues = tTargetVals;
				}

#ifdef RG_CUDA_SUPPORT
				if (ppo->device.is_cuda())
					c10::cuda::CUDACachingAllocator::emptyCache();
#endif

				Timer learnTimer = {};
				ppo->Learn(experience, report, isFirstIteration);
				report["PPO Learn Time"] = learnTimer.Elapsed();

				float consumptionTime = consumptionTimer.Elapsed();
				report["Collection Time"] = collectionTime;
				report["Consumption Time"] = consumptionTime;
				report["Collection Steps/Second"] = stepsCollected / collectionTime;
				report["Consumption Steps/Second"] = stepsCollected / consumptionTime;
				report["Overall Steps/Second"] = stepsCollected / (collectionTime + consumptionTime);

				uint64_t prevTimesteps = totalTimesteps;
				totalTimesteps += stepsCollected;
				report["Total Timesteps"] = totalTimesteps;
				totalIterations++;
				report["Total Iterations"] = totalIterations;

				if (versionMgr)
					versionMgr->OnIteration(ppo, report, totalTimesteps, prevTimesteps);

				// Auto-save
				if (!config.checkpointFolder.empty()) {
					if (totalTimesteps / config.tsPerSave > prevTimesteps / config.tsPerSave)
						Save();
				}

				report.Finish();

				// Update metrics snapshot for UI
				{
					std::lock_guard<std::mutex> lock(metricsMutex);
					latestMetrics.totalTimesteps = totalTimesteps;
					latestMetrics.totalIterations = totalIterations;
					if (report.Has("Overall Steps/Second")) latestMetrics.overallSPS = report["Overall Steps/Second"];
					if (report.Has("Collection Steps/Second")) latestMetrics.collectionSPS = report["Collection Steps/Second"];
					if (report.Has("Consumption Steps/Second")) latestMetrics.consumptionSPS = report["Consumption Steps/Second"];
					if (report.Has("Average Step Reward")) latestMetrics.avgStepReward = report["Average Step Reward"];
					if (report.Has("Policy Entropy")) latestMetrics.policyEntropy = report["Policy Entropy"];
					if (report.Has("Policy Loss")) latestMetrics.policyLoss = report["Policy Loss"];
					if (report.Has("Critic Loss")) latestMetrics.criticLoss = report["Critic Loss"];
					if (report.Has("Mean KL Divergence")) latestMetrics.klDivergence = report["Mean KL Divergence"];
					if (report.Has("SB3 Clip Fraction")) latestMetrics.clipFraction = report["SB3 Clip Fraction"];
					if (report.Has("Policy Update Magnitude")) latestMetrics.policyUpdateMag = report["Policy Update Magnitude"];
					if (report.Has("Critic Update Magnitude")) latestMetrics.criticUpdateMag = report["Critic Update Magnitude"];
					if (report.Has("Collection Time")) latestMetrics.collectionTime = report["Collection Time"];
					if (report.Has("Consumption Time")) latestMetrics.consumptionTime = report["Consumption Time"];
					if (report.Has("GAE Time")) latestMetrics.gaeTime = report["GAE Time"];
					if (report.Has("PPO Learn Time")) latestMetrics.ppoLearnTime = report["PPO Learn Time"];

					// Extract reward breakdown
					latestMetrics.rewardBreakdown.clear();
					for (auto& pair : report.data) {
						if (pair.first.starts_with("Rewards/"))
							latestMetrics.rewardBreakdown[pair.first.substr(8)] = pair.second;
					}

					// Extract skill metrics
					latestMetrics.skillMetrics.clear();
					for (auto& pair : report.data) {
						if (pair.first.starts_with("Nigel/") || pair.first.starts_with("Player/") || pair.first.starts_with("Game/"))
							latestMetrics.skillMetrics[pair.first] = pair.second;
					}
				}

				report.Display({
					"Average Step Reward",
					"Policy Entropy",
					"KL Div Loss",
					"",
					"Policy Update Magnitude",
					"Critic Update Magnitude",
					"Shared Head Update Magnitude",
					"",
					"Collection Steps/Second",
					"Consumption Steps/Second",
					"Overall Steps/Second",
					"",
					"Collection Time",
					"-Inference Time",
					"-Env Step Time",
					"Consumption Time",
					"-GAE Time",
					"-PPO Learn Time",
					"",
					"Collected Timesteps",
					"Total Timesteps",
					"Total Iterations"
				});
			}
		}

	} catch (std::exception& e) {
		RG_ERR_CLOSE("Exception thrown during training: " << e.what());
	}

	state = TrainerState::STOPPED;
}

void NGL::Trainer::RequestPause() { state = TrainerState::PAUSED; }
void NGL::Trainer::RequestResume() { state = TrainerState::RUNNING; }
void NGL::Trainer::RequestSave() { saveRequested = true; }
void NGL::Trainer::RequestStop() { stopRequested = true; }

NGL::MetricsSnapshot NGL::Trainer::GetLatestMetrics() {
	std::lock_guard<std::mutex> lock(metricsMutex);
	return latestMetrics;
}

std::vector<NGL::Trainer::RewardInfo> NGL::Trainer::GetRewardWeights() {
	std::vector<RewardInfo> result;
	if (!envSet || envSet->rewards.empty()) return result;
	// All arenas have the same reward set, just read from arena 0
	for (auto& wr : envSet->rewards[0])
		result.push_back({ wr.reward->GetName(), wr.weight });
	return result;
}

void NGL::Trainer::SetRewardWeight(const std::string& name, float weight) {
	if (!envSet) return;
	// Update across all arenas
	for (auto& arenaRewards : envSet->rewards)
		for (auto& wr : arenaRewards)
			if (wr.reward->GetName() == name)
				wr.weight = weight;
}

void NGL::Trainer::SaveStats(std::filesystem::path path) {
	using namespace nlohmann;
	std::ofstream fOut(path);
	if (!fOut.good()) RG_ERR_CLOSE("Trainer::SaveStats(): Can't open file at " << path);

	json j = {};
	j["total_timesteps"] = totalTimesteps;
	j["total_iterations"] = totalIterations;
	if (returnStat) j["return_stat"] = returnStat->ToJSON();
	if (obsStat) j["obs_stat"] = obsStat->ToJSON();
	if (versionMgr) versionMgr->AddRunningStatsToJSON(j);

	fOut << j.dump(4);
}

void NGL::Trainer::LoadStats(std::filesystem::path path) {
	using namespace nlohmann;
	std::ifstream fIn(path);
	if (!fIn.good()) RG_ERR_CLOSE("Trainer::LoadStats(): Can't open file at " << path);

	json j = json::parse(fIn);
	totalTimesteps = j["total_timesteps"];
	totalIterations = j["total_iterations"];
	if (returnStat && j.contains("return_stat")) returnStat->ReadFromJSON(j["return_stat"]);
	if (obsStat && j.contains("obs_stat")) obsStat->ReadFromJSON(j["obs_stat"]);
	if (versionMgr) versionMgr->LoadRunningStatsFromJSON(j);
}

void NGL::Trainer::Save() {
	if (config.checkpointFolder.empty()) RG_ERR_CLOSE("Trainer::Save(): No checkpoint folder set");

	std::filesystem::path saveFolder = config.checkpointFolder / std::to_string(totalTimesteps);
	std::filesystem::create_directories(saveFolder);

	RG_LOG("Saving to folder " << saveFolder << "...");
	SaveStats(saveFolder / STATS_FILE_NAME);
	ppo->SaveTo(saveFolder);

	if (config.checkpointsToKeep != -1) {
		std::set<int64_t> allSavedTimesteps = Utils::FindNumberedDirs(config.checkpointFolder);
		while (allSavedTimesteps.size() > config.checkpointsToKeep) {
			int64_t lowest = *allSavedTimesteps.begin();
			try {
				std::filesystem::remove_all(config.checkpointFolder / std::to_string(lowest));
			} catch (std::exception& e) {
				RG_ERR_CLOSE("Failed to remove old checkpoint: " << e.what());
			}
			allSavedTimesteps.erase(lowest);
		}
	}

	if (versionMgr) versionMgr->SaveVersions();
	lastSavedTimesteps = totalTimesteps;
	RG_LOG(" > Done.");
}

void NGL::Trainer::Load() {
	if (config.checkpointFolder.empty()) RG_ERR_CLOSE("Trainer::Load(): No checkpoint folder set");

	RG_LOG("Loading most recent checkpoint in " << config.checkpointFolder << "...");
	int64_t highest = -1;
	std::set<int64_t> allSavedTimesteps = Utils::FindNumberedDirs(config.checkpointFolder);
	for (int64_t ts : allSavedTimesteps) highest = RS_MAX(ts, highest);

	if (highest != -1) {
		std::filesystem::path loadFolder = config.checkpointFolder / std::to_string(highest);
		RG_LOG(" > Loading checkpoint " << loadFolder << "...");
		LoadStats(loadFolder / STATS_FILE_NAME);
		ppo->LoadFrom(loadFolder);
		lastLoadedTimesteps = highest;
		RG_LOG(" > Done.");
	} else {
		RG_LOG(" > No checkpoints found, starting new model.");
	}
}

NGL::Trainer::~Trainer() {
	delete ppo;
	delete versionMgr;
	delete renderSender;
	delete returnStat;
	delete obsStat;
	delete envSet;
}
