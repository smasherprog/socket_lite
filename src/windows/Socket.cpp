#include "Socket.h"

namespace SL {
namespace NET {
    SOCKET Socket::Create(NetworkProtocol protocol)
    {
        auto type = protocol == NetworkProtocol::IPV4 ? AF_INET : AF_INET6;
        if (auto handle = WSASocketW(type, SOCK_STREAM, IPPROTO_IP, NULL, 0, WSA_FLAG_OVERLAPPED); handle != INVALID_SOCKET) {
            auto nZero = 0;
            if (auto nRet = setsockopt(handle, SOL_SOCKET, SO_SNDBUF, (char *)&nZero, sizeof(nZero)); nRet == SOCKET_ERROR) {
                closesocket(handle);
                return INVALID_SOCKET;
            }
            u_long iMode = 1; // set socket for non blocking
            if (auto nRet = ioctlsocket(handle, FIONBIO, &iMode); nRet == NO_ERROR) {
                closesocket(handle);
                return INVALID_SOCKET;
            }
            return handle;
        }
        return INVALID_SOCKET;
    }
    Socket::Socket(NetworkProtocol protocol) { handle = Create(protocol); }
    Socket::~Socket() { close(); }
    void Socket::close()
    {
        if (handle != INVALID_SOCKET) {
            closesocket(handle);
        }
        handle = INVALID_SOCKET;
    }

} // namespace NET
} // namespace SL
