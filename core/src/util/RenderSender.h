#pragma once
#include <RLGymCPP/Gamestates/GameState.h>
#include <string>
#include <cstdint>
#include <array>

namespace NGL {
	class RenderSender {
	public:
		RenderSender(float timeScale);
		~RenderSender();

		void Send(const RLGC::GameState& state);

	private:
		float m_timeScale;
		double m_adaptiveDelay = -1;
		double m_lastSendTime = 0;

		// Opaque socket handle (avoids including winsock2.h here)
		uintptr_t m_socket = ~(uintptr_t)0; // INVALID_SOCKET
		std::array<char, 16> m_addr = {};    // sockaddr_in storage
		bool m_wsaInit = false;

		std::string GameStateToJSON(const RLGC::GameState& state);
		double GetTimeSeconds() const;
	};
}
