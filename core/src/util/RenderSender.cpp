#include "RenderSender.h"

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#endif

#include <nlohmann/json.hpp>
#include <chrono>
#include <thread>

using namespace nlohmann;
using namespace RLGC;

NGL::RenderSender::RenderSender(float timeScale) : m_timeScale(timeScale) {
#ifdef _WIN32
	WSADATA wsaData;
	m_wsaInit = (WSAStartup(MAKEWORD(2, 2), &wsaData) == 0);
	if (!m_wsaInit) return;

	SOCKET sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	m_socket = (uintptr_t)sock;
	if (sock == INVALID_SOCKET) return;

	sockaddr_in addr = {};
	addr.sin_family = AF_INET;
	addr.sin_port = htons(9273);
	inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);
	static_assert(sizeof(addr) <= 16);
	memcpy(m_addr.data(), &addr, sizeof(addr));
#endif
}

NGL::RenderSender::~RenderSender() {
#ifdef _WIN32
	SOCKET sock = (SOCKET)m_socket;
	if (sock != INVALID_SOCKET) closesocket(sock);
	if (m_wsaInit) WSACleanup();
#endif
}

double NGL::RenderSender::GetTimeSeconds() const {
	using namespace std::chrono;
	return duration<double>(steady_clock::now().time_since_epoch()).count();
}

static json VecToJSON(const Vec& v) {
	return json::array({ v.x, v.y, v.z });
}

static json PhysToJSON(const PhysState& p) {
	json j;
	j["pos"] = VecToJSON(p.pos);
	j["forward"] = VecToJSON(p.rotMat.forward);
	j["right"] = VecToJSON(p.rotMat.right);
	j["up"] = VecToJSON(p.rotMat.up);
	j["vel"] = VecToJSON(p.vel);
	j["ang_vel"] = VecToJSON(p.angVel);
	return j;
}

std::string NGL::RenderSender::GameStateToJSON(const GameState& state) {
	json j;
	j["gamemode"] = "soccar";

	// Ball (strip rotation vectors like the Python receiver does)
	json ball;
	ball["pos"] = VecToJSON(state.ball.pos);
	ball["vel"] = VecToJSON(state.ball.vel);
	ball["ang_vel"] = VecToJSON(state.ball.angVel);
	j["ball_phys"] = ball;

	// Cars
	json cars = json::array();
	for (auto& player : state.players) {
		json car;
		car["car_id"] = player.carId;
		car["team_num"] = (int)player.team;
		car["phys"] = PhysToJSON(player);
		car["is_demoed"] = player.isDemoed;
		car["on_ground"] = player.isOnGround;
		car["ball_touched"] = player.ballTouchedStep;
		car["has_flip"] = player.HasFlipOrJump();
		car["boost_amount"] = player.boost / 100.0f;
		cars.push_back(car);
	}
	j["cars"] = cars;

	// Boost pads
	j["boost_pad_states"] = state.boostPads;

	return j.dump();
}

void NGL::RenderSender::Send(const GameState& state) {
#ifdef _WIN32
	SOCKET sock = (SOCKET)m_socket;
	if (sock == INVALID_SOCKET) return;

	std::string data = GameStateToJSON(state);
	sendto(sock, data.c_str(), (int)data.size(), 0,
		(const sockaddr*)m_addr.data(), sizeof(sockaddr_in));

	// Adaptive delay to sync rendering speed
	double now = GetTimeSeconds();
	double targetDelay = state.deltaTime / m_timeScale;

	if (m_lastSendTime > 0) {
		double realDelay = now - m_lastSendTime;
		double error = targetDelay - realDelay;

		if (m_adaptiveDelay < 0)
			m_adaptiveDelay = targetDelay;
		else
			m_adaptiveDelay += error * 0.3;

		if (m_adaptiveDelay < 0) m_adaptiveDelay = 0;
		if (m_adaptiveDelay > targetDelay) m_adaptiveDelay = targetDelay;

		int64_t sleepUs = (int64_t)(m_adaptiveDelay * 1'000'000);
		if (sleepUs > 0)
			std::this_thread::sleep_for(std::chrono::microseconds(sleepUs));
	}

	m_lastSendTime = GetTimeSeconds();
#endif
}
