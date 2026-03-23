#pragma once

// Include RLGymCPP (transitively includes RocketSim, filesystem, mutex, chrono, etc.)
#include <RLGymCPP/EnvSet/EnvSet.h>
#include <RLGymCPP/BasicTypes/Lists.h>
#include <atomic>
using RLGC::FList;
using RLGC::IList;

#define RG_SLEEP(ms) std::this_thread::sleep_for(std::chrono::milliseconds(ms))
#define THREAD_WAIT() RG_SLEEP(2)
