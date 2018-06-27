
#include "Socket_Lite.h"
#include "defs.h"
#include <assert.h>
#ifdef _WIN32
#include <WinSock2.h>
#include <Windows.h>
#include <Ws2tcpip.h>
#include <mswsock.h>
typedef int SOCKLEN_T;
#else
#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
typedef socklen_t SOCKLEN_T;
#endif

#include <string.h>
#include <string>

namespace SL {
namespace NET {

 

    const unsigned char *SocketAddress::getSocketAddr() const { return SocketImpl; }
    int SocketAddress::getSocketAddrLen() const { return SocketImplLen; }
    const std::string &SocketAddress::getHost() const { return Host; }
    unsigned short SocketAddress::getPort() const { return Port; }
    AddressFamily SocketAddress::getFamily() const { return Family; }

    const unsigned char *SocketAddr(const SocketAddress &s) { return s.getSocketAddr(); }
    int SocketAddrLen(const SocketAddress &s) { return s.getSocketAddrLen(); }
    const std::string &Host(const SocketAddress &s) { return s.getHost(); }
    unsigned short Port(const SocketAddress &s) { return s.getPort(); }
    AddressFamily Family(const SocketAddress &s) { return s.getFamily(); }

    SocketAddress::SocketAddress(const SocketAddress &addr) : SocketImplLen(addr.SocketImplLen), Host(addr.Host), Port(addr.Port), Family(addr.Family)
    {
        memcpy(SocketImpl, addr.SocketImpl, sizeof(SocketImpl));
    }
    SocketAddress::SocketAddress(unsigned char *buffer, int len, const char *host, unsigned short port, AddressFamily family)
    {
        assert(static_cast<size_t>(len) < sizeof(SocketImpl));
        memcpy(SocketImpl, buffer, len);
        SocketImplLen = len;
        Host = host;
        Port = port;
        Family = family;
    }

    std::vector<SocketAddress> getaddrinfo(const char *nodename, PortNumber pServiceName, AddressFamily family)
    {
        ::addrinfo hints = {0};
        ::addrinfo *result(nullptr);
        memset(&hints, 0, sizeof(addrinfo));
        hints.ai_family = AF_UNSPEC;     /* Return IPv4 and IPv6 choices */
        hints.ai_socktype = SOCK_STREAM; /* We want a TCP socket */
        hints.ai_protocol = IPPROTO_TCP;
        hints.ai_flags = AI_PASSIVE; /* All interfaces */
        std::vector<SocketAddress> addrs;
        auto portstr = std::to_string(pServiceName.value);
        auto s = ::getaddrinfo(nodename, portstr.c_str(), &hints, &result);
        if (s != 0) {
            return addrs;
        } 
        for (auto ptr = result; ptr != NULL; ptr = ptr->ai_next) {

            char str[INET_ADDRSTRLEN];
            if (ptr->ai_family == AF_INET6 && (family == AddressFamily::IPV6 || family == AddressFamily::IPANY)) {
                auto so = (::sockaddr_in6 *)ptr->ai_addr;
                inet_ntop(AF_INET6, &so->sin6_addr, str, INET_ADDRSTRLEN); 
                addrs.emplace_back(SocketAddress{(unsigned char *)so, sizeof(sockaddr_in6), str, so->sin6_port, AddressFamily::IPV6});
            }
            else if (ptr->ai_family == AF_INET && (family == AddressFamily::IPV4 || family == AddressFamily::IPANY)) {
                auto so = (::sockaddr_in *)ptr->ai_addr;
                inet_ntop(AF_INET, &so->sin_addr, str, INET_ADDRSTRLEN);
                addrs.emplace_back(SocketAddress{(unsigned char *)so, sizeof(sockaddr_in), str, so->sin_port, AddressFamily::IPV4});
            }
        }
        freeaddrinfo(result);
        return addrs;
    }
    StatusCode TranslateError(int *errcode)
    {
#if _WIN32
        auto originalerr = WSAGetLastError();
        auto errorcode = errcode != nullptr ? *errcode : originalerr;
        switch (errorcode) {
        case WSAECONNRESET:
            return StatusCode::SC_ECONNRESET;
        case WSAETIMEDOUT:
        case WSAECONNABORTED:
            return StatusCode::SC_ETIMEDOUT;
        case WSAEWOULDBLOCK:
            return StatusCode::SC_EWOULDBLOCK;
#else
        auto originalerror = errno;
        auto errorcode = errcode != nullptr ? *errcode : originalerror;
        switch (errorcode) {

#endif

        default:
            return StatusCode::SC_CLOSED;
        };
    } // namespace NET

} // namespace NET
} // namespace SL
