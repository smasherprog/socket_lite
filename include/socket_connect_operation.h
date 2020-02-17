///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Lewis Baker
// Licenced under MIT license. See LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////
#ifndef SL_NETWORK_SOCKET_CONNECT_OP
#define SL_NETWORK_SOCKET_CONNECT_OP

#include "ip_endpoint.h"
#include "utils.h"

#if _WIN32

namespace SL::Network {

class socket;
class socket_connect_operation : public win32::overlapped_operation<socket_connect_operation> {
  public:
    socket_connect_operation(socket &socket, const ip_endpoint &remoteEndPoint) noexcept : m_socket(socket), m_remoteEndPoint(remoteEndPoint) {}

  private:
    friend class win32::overlapped_operation<socket_connect_operation>;

    bool try_start();
    decltype(auto) get_result();

  private:
    socket &m_socket;
    ip_endpoint m_remoteEndPoint;
};
} // namespace SL::Network

#endif

#endif
