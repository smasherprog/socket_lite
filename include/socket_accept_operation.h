#ifndef SL_NETWORK_SOCKET_ACCEPT
#define SL_NETWORK_SOCKET_ACCEPT

#if _WIN32

#include "utils.h"
#include <atomic>
#include <experimental/coroutine>
#include <optional>

namespace SL::Network {
class socket;

class socket_accept_operation : public win32::overlapped_operation {
  public:
    socket_accept_operation(socket &listeningSocket, socket &acceptingSocket) noexcept
        : m_listeningSocket(listeningSocket), m_acceptingSocket(acceptingSocket)
    {
    }
    bool await_ready() noexcept;
    bool await_suspend(std::experimental::coroutine_handle<> awaitingCoroutine)
    {
        m_awaitingCoroutine = awaitingCoroutine;
        return try_start();
    }

    void await_resume();

  private:
    bool socket_accept_operation::try_start() noexcept;
    void get_result();

    socket &m_listeningSocket;
    socket &m_acceptingSocket;
    std::uint8_t m_addressBuffer[88];
};
} // namespace SL::Network

#endif
#endif
