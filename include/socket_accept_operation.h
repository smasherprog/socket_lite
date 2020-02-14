#ifndef SL_NETWORK_SOCKET_ACCEPT
#define SL_NETWORK_SOCKET_ACCEPT

#include "utils.h"

#if _WIN32

# include <atomic>
# include <optional>
# include <experimental/coroutine>

namespace SL::Network {
	class socket;
	class socket_accept_operation : public win32::overlapped_operation
	{
	public:
		socket_accept_operation(socket& listeningSocket, socket& acceptingSocket) noexcept : m_listeningSocket(listeningSocket), m_acceptingSocket(acceptingSocket) {}

	private:
		socket& m_listeningSocket;
		socket& m_acceptingSocket;
		std::uint8_t m_addressBuffer[88];

	};
}

#endif
#endif
