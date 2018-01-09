
#include "Socket_Lite.h"
#include <assert.h>

#ifdef WIN32
#include <WinSock2.h>
#include <Windows.h>
#include <Ws2tcpip.h>
#include <mswsock.h>

#else
#include <sys/socket.h>
#include <sys/types.h>
#define INVALID_SOCKET -1
#define closesocket close
#endif
#include <iostream>
#include <string>

namespace SL {
namespace NET {

    std::vector<SL::NET::sockaddr> getaddrinfo(char *nodename, PortNumber pServiceName, Address_Family family)
    {
        ::addrinfo hints = {0};
        ::addrinfo *result(nullptr);
        std::vector<SL::NET::sockaddr> ret;
        memset(&hints, 0, sizeof(addrinfo));
        hints.ai_family = AF_UNSPEC;     /* Return IPv4 and IPv6 choices */
        hints.ai_socktype = SOCK_STREAM; /* We want a TCP socket */
        hints.ai_protocol = IPPROTO_TCP;
        hints.ai_flags = AI_PASSIVE; /* All interfaces */
        auto portstr = std::to_string(pServiceName.value);
        auto s = ::getaddrinfo(nodename, portstr.c_str(), &hints, &result);
        if (s != 0) {
            std::cerr << "getaddrinfo" << gai_strerror(s) << std::endl;
            return ret;
        }
        for (auto ptr = result; ptr != NULL; ptr = ptr->ai_next) {

            if (ptr->ai_family == AF_INET6 && family == Address_Family::IPV6) {
                auto so = (sockaddr_in6 *)&ptr->ai_addr;
                SL::NET::sockaddr tmp;
                inet_ntop(AF_INET6, &so->sin6_addr, tmp.Address, sizeof(tmp.Address));
                tmp.Port = ntohs(so->sin6_port);
                tmp.Family = Address_Family::IPV6;
                ret.push_back(tmp);
            }
            else if (ptr->ai_family == AF_INET && family == Address_Family::IPV4) {
                auto so = (::sockaddr_in *)&ptr->ai_addr;
                SL::NET::sockaddr tmp;
                inet_ntop(AF_INET, &so->sin_addr, tmp.Address, sizeof(tmp.Address));
                tmp.Port = ntohs(so->sin_port);
                tmp.Family = Address_Family::IPV4;
                ret.push_back(tmp);
            }
        }
        freeaddrinfo(result);
        return ret;
    } // namespace NET

    bool ISocket::bind_(Platform_Socket s, sockaddr addr) const
    {
        if (addr.Family == Address_Family::IPV4) {
            ::sockaddr_in sockaddr = {0};
            int sockaddrlen = sizeof(sockaddr);
            inet_pton(AF_INET, addr.Address, &sockaddr.sin_addr);
            return ::bind(s, (::sockaddr *)&sockaddr, sockaddrlen) == 0;
        }
        else {
            sockaddr_in6 sockaddr = {0};
            int sockaddrlen = sizeof(sockaddr);
            inet_pton(AF_INET6, addr.Address, &sockaddr.sin6_addr);
            return ::bind(s, (::sockaddr *)&sockaddr, sockaddrlen) == 0;
        }
    }

    std::optional<SL::NET::sockaddr> ISocket::getpeername_(Platform_Socket s) const
    {
        sockaddr_storage addr = {0};
        socklen_t len = sizeof(addr);
        if (::getpeername(s, (::sockaddr *)&addr, &len) == 0) {
            // deal with both IPv4 and IPv6:
            if (addr.ss_family == AF_INET) {
                auto so = (::sockaddr_in *)&addr;
                SL::NET::sockaddr tmp;
                inet_ntop(AF_INET, &so->sin_addr, tmp.Address, sizeof(tmp.Address));
                tmp.Port = ntohs(so->sin_port);
                tmp.Family = Address_Family::IPV4;
                return std::optional<SL::NET::sockaddr>(tmp);
            }
            else { // AF_INET6
                auto so = (sockaddr_in6 *)&addr;
                SL::NET::sockaddr tmp;
                inet_ntop(AF_INET6, &so->sin6_addr, tmp.Address, sizeof(tmp.Address));
                tmp.Port = ntohs(so->sin6_port);
                tmp.Family = Address_Family::IPV6;
                return std::optional<SL::NET::sockaddr>(tmp);
            }
        }
        return std::nullopt;
    }
    std::optional<SL::NET::sockaddr> ISocket::getsockname_(Platform_Socket s) const
    {
        sockaddr_storage addr = {0};
        socklen_t len = sizeof(addr);
        if (::getsockname(s, (::sockaddr *)&addr, &len) == 0) {
            // deal with both IPv4 and IPv6:
            if (addr.ss_family == AF_INET) {
                auto so = (::sockaddr_in *)&addr;
                SL::NET::sockaddr tmp;
                inet_ntop(AF_INET, &so->sin_addr, tmp.Address, sizeof(tmp.Address));
                tmp.Port = ntohs(so->sin_port);
                tmp.Family = Address_Family::IPV4;
                return std::optional<SL::NET::sockaddr>(tmp);
            }
            else { // AF_INET6
                auto so = (::sockaddr_in6 *)&addr;
                SL::NET::sockaddr tmp;
                inet_ntop(AF_INET6, &so->sin6_addr, tmp.Address, sizeof(tmp.Address));
                tmp.Port = ntohs(so->sin6_port);
                tmp.Family = Address_Family::IPV6;
                return std::optional<SL::NET::sockaddr>(tmp);
            }
        }
        return std::nullopt;
    }
    std::optional<bool> ISocket::getsockopt_O_DEBUG(Platform_Socket s) const
    {
        int value = 0;
        int valuelen = sizeof(value);
        if (::getsockopt(s, SOL_SOCKET, SO_DEBUG, (char *)&value, &valuelen) == 0) {
            return std::optional<bool>(value != 0);
        }
        return std::nullopt;
    }

    std::optional<bool> ISocket::getsockopt_O_ACCEPTCONN(Platform_Socket s) const
    {
        int value = 0;
        int valuelen = sizeof(value);
        if (::getsockopt(s, SOL_SOCKET, SO_ACCEPTCONN, (char *)&value, &valuelen) == 0) {
            return std::optional<bool>(value != 0);
        }
        return std::nullopt;
    }

    std::optional<bool> ISocket::getsockopt_O_BROADCAST(Platform_Socket s) const
    {
        int value = 0;
        int valuelen = sizeof(value);
        if (::getsockopt(s, SOL_SOCKET, SO_BROADCAST, (char *)&value, &valuelen) == 0) {
            return std::optional<bool>(value != 0);
        }
        return std::nullopt;
    }

    std::optional<bool> ISocket::getsockopt_O_REUSEADDR(Platform_Socket s) const
    {
        int value = 0;
        int valuelen = sizeof(value);
        if (::getsockopt(s, SOL_SOCKET, SO_REUSEADDR, (char *)&value, &valuelen) == 0) {
            return std::optional<bool>(value != 0);
        }
        return std::nullopt;
    }

    std::optional<bool> ISocket::getsockopt_O_KEEPALIVE(Platform_Socket s) const
    {
        int value = 0;
        int valuelen = sizeof(value);
        if (::getsockopt(s, SOL_SOCKET, SO_KEEPALIVE, (char *)&value, &valuelen) == 0) {
            return std::optional<bool>(value != 0);
        }
        return std::nullopt;
    }

    std::optional<Linger_Option> ISocket::getsockopt_O_LINGER(Platform_Socket s) const
    {
        linger value;
        int valuelen = sizeof(value);
        if (::getsockopt(s, SOL_SOCKET, SO_LINGER, (char *)&value, &valuelen) == 0) {
            return std::optional<Linger_Option>(
                {value.l_onoff == 0 ? Linger_Options::LINGER_OFF : Linger_Options::LINGER_ON, std::chrono::seconds(value.l_linger)});
        }
        return std::nullopt;
    }

    std::optional<bool> ISocket::getsockopt_O_OOBINLINE(Platform_Socket s) const
    {
        int value = 0;
        int valuelen = sizeof(value);
        if (::getsockopt(s, SOL_SOCKET, SO_OOBINLINE, (char *)&value, &valuelen) == 0) {
            return std::optional<bool>(value != 0);
        }
        return std::nullopt;
    }

    std::optional<bool> ISocket::getsockopt_O_EXCLUSIVEADDRUSE(Platform_Socket s) const
    {
        int value = 0;
        int valuelen = sizeof(value);
        if (::getsockopt(s, SOL_SOCKET, SO_EXCLUSIVEADDRUSE, (char *)&value, &valuelen) == 0) {
            return std::optional<bool>(value != 0);
        }
        return std::nullopt;
    }

    std::optional<int> ISocket::getsockopt_O_SNDBUF(Platform_Socket s) const
    {
        int value = 0;
        int valuelen = sizeof(value);
        if (::getsockopt(s, SOL_SOCKET, SO_SNDBUF, (char *)&value, &valuelen) == 0) {
            return std::optional<int>(value);
        }
        return std::nullopt;
    }

    std::optional<int> ISocket::getsockopt_O_RCVBUF(Platform_Socket s) const
    {
        int value = 0;
        int valuelen = sizeof(value);
        if (::getsockopt(s, SOL_SOCKET, SO_RCVBUF, (char *)&value, &valuelen) == 0) {
            return std::optional<int>(value);
        }
        return std::nullopt;
    }

    std::optional<std::chrono::seconds> ISocket::getsockopt_O_SNDTIMEO(Platform_Socket s) const
    {
#ifdef WIN32
        DWORD value = 0;
        int valuelen = sizeof(value);
        if (::getsockopt(s, SOL_SOCKET, SO_SNDTIMEO, (char *)&value, &valuelen) == 0) {
            return std::optional<std::chrono::seconds>(value * 1000); // convert from ms to seconds
        }
#else
        timeval value = {0};
        int valuelen = sizeof(value);
        if (::getsockopt(s, SOL_SOCKET, SO_SNDTIMEO, (char *)&value, &valuelen) == 0) {
            return std::optional<std::chrono::seconds>(value.tv_sec); // convert from ms to seconds
        }
#endif
        return std::nullopt;
    }

    std::optional<std::chrono::seconds> ISocket::getsockopt_O_RCVTIMEO(Platform_Socket s) const
    {
#ifdef WIN32
        DWORD value = 0;
        int valuelen = sizeof(value);
        if (::getsockopt(s, SOL_SOCKET, SO_RCVTIMEO, (char *)&value, &valuelen) == 0) {
            return std::optional<std::chrono::seconds>(value * 1000); // convert from ms to seconds
        }
#else
        timeval value = {0};
        int valuelen = sizeof(value);
        if (::getsockopt(s, SOL_SOCKET, SO_RCVTIMEO, (char *)&value, &valuelen) == 0) {
            return std::optional<std::chrono::seconds>(value.tv_sec); // convert from ms to seconds
        }
#endif
        return std::nullopt;
    }

    std::optional<int> ISocket::getsockopt_O_ERROR(Platform_Socket s) const
    {
        int value = 0;
        int valuelen = sizeof(value);
        if (::getsockopt(s, SOL_SOCKET, SO_ERROR, (char *)&value, &valuelen) == 0) {
            return std::optional<int>(value);
        }
        return std::nullopt;
    }

    std::optional<bool> ISocket::getsockopt_O_NODELAY(Platform_Socket s) const
    {
        int value = 0;
        int valuelen = sizeof(value);
        if (::getsockopt(s, IPPROTO_TCP, TCP_NODELAY, (char *)&value, &valuelen) == 0) {
            return std::optional<bool>(value != 0);
        }
        return std::nullopt;
    }

    std::optional<Blocking_Options> ISocket::getsockopt_O_BLOCKING(Platform_Socket s) const
    {
#ifdef WIN32
        // not supported on windows
        return std::nullopt;
#else
        long arg = 0;
        if ((arg = fcntl(s, F_GETFL, NULL)) < 0) {
            return std::nullopt;
        }
        return std::optional<Blocking_Options>((arg & O_NONBLOCK) != 0 ? Blocking_Options::NON_BLOCKING : Blocking_Options::BLOCKING);
#endif
    }

    bool ISocket::setsockopt_O_DEBUG(Platform_Socket s, bool b) const
    {
        int value = b ? 1 : 0;
        int valuelen = sizeof(value);
        return ::setsockopt(s, SOL_SOCKET, SO_DEBUG, (char *)&value, valuelen) == 0;
    }

    bool ISocket::setsockopt_O_BROADCAST(Platform_Socket s, bool b) const
    {
        int value = b ? 1 : 0;
        int valuelen = sizeof(value);
        return ::setsockopt(s, SOL_SOCKET, SO_BROADCAST, (char *)&value, valuelen) == 0;
    }

    bool ISocket::setsockopt_O_REUSEADDR(Platform_Socket s, bool b) const
    {
        int value = b ? 1 : 0;
        int valuelen = sizeof(value);
        return ::setsockopt(s, SOL_SOCKET, SO_BROADCAST, (char *)&value, valuelen) == 0;
    }

    bool ISocket::setsockopt_O_KEEPALIVE(Platform_Socket s, bool b) const
    {
        int value = b ? 1 : 0;
        int valuelen = sizeof(value);
        return ::setsockopt(s, SOL_SOCKET, SO_KEEPALIVE, (char *)&value, valuelen) == 0;
    }

    bool ISocket::setsockopt_O_LINGER(Platform_Socket s, Linger_Option o) const
    {
        linger value;
        value.l_onoff = o.l_onoff == Linger_Options::LINGER_OFF ? 0 : 1;
        value.l_linger = static_cast<unsigned short>(o.l_linger.count());
        int valuelen = sizeof(value);
        return ::setsockopt(s, SOL_SOCKET, SO_LINGER, (char *)&value, valuelen) == 0;
    }

    bool ISocket::setsockopt_O_OOBINLINE(Platform_Socket s, bool b) const
    {
        int value = b ? 1 : 0;
        int valuelen = sizeof(value);
        return ::setsockopt(s, SOL_SOCKET, SO_OOBINLINE, (char *)&value, valuelen) == 0;
    }

    bool ISocket::setsockopt_O_EXCLUSIVEADDRUSE(Platform_Socket s, bool b) const
    {
        int value = b ? 1 : 0;
        int valuelen = sizeof(value);
        return ::setsockopt(s, SOL_SOCKET, SO_EXCLUSIVEADDRUSE, (char *)&value, valuelen) == 0;
    }

    bool ISocket::setsockopt_O_SNDBUF(Platform_Socket s, int b) const
    {
        int value = b;
        int valuelen = sizeof(value);
        return ::setsockopt(s, SOL_SOCKET, SO_SNDBUF, (char *)&value, valuelen) == 0;
    }

    bool ISocket::setsockopt_O_RCVBUF(Platform_Socket s, int b) const
    {
        int value = b;
        int valuelen = sizeof(value);
        return ::setsockopt(s, SOL_SOCKET, SO_RCVBUF, (char *)&value, valuelen) == 0;
    }

    bool ISocket::setsockopt_O_SNDTIMEO(Platform_Socket s, std::chrono::seconds sec) const
    {
#ifdef WIN32
        DWORD value = static_cast<DWORD>(sec.count() * 1000); // convert to miliseconds for windows
        int valuelen = sizeof(value);
        return ::setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, (char *)&value, valuelen) == 0;
#else
        timeval tv = {0};
        tv.tv_sec = static_cast<long>(sec.count());
        return ::setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, (timeval *)&tv, sizeof(tv)) == 0;
#endif
    }

    bool ISocket::setsockopt_O_RCVTIMEO(Platform_Socket s, std::chrono::seconds sec) const
    {
#ifdef WIN32
        DWORD value = static_cast<DWORD>(sec.count() * 1000); // convert to miliseconds for windows
        int valuelen = sizeof(value);
        return ::setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, (char *)&value, valuelen) == 0;
#else
        timeval tv = {0};
        tv.tv_sec = static_cast<long>(sec.count());
        return ::setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, (timeval *)&tv, sizeof(tv)) == 0;
#endif
    }

    bool ISocket::setsockopt_O_NODELAY(Platform_Socket s, bool b) const
    {
        int value = b ? 1 : 0;
        int valuelen = sizeof(value);
        return ::setsockopt(s, IPPROTO_TCP, TCP_NODELAY, (char *)&value, valuelen) == 0;
    }

    bool ISocket::setsockopt_O_BLOCKING(Platform_Socket s, Blocking_Options b) const
    {

#ifdef WIN32
        u_long iMode = b == Blocking_Options::BLOCKING ? 0 : 1;
        if (auto nRet = ioctlsocket(s, FIONBIO, &iMode); nRet != NO_ERROR) {
            return false;
        }
#else
        int flags = fcntl(s, F_GETFL, 0);
        if (flags == -1)
            return false;
        flags = b == Blocking_Options::BLOCKING ? (flags & ~O_NONBLOCK) : (flags | O_NONBLOCK);
        return (fcntl(s, F_SETFL, flags) == 0) ? true : false;
    }
#endif
        return true;
    } // namespace NET

} // namespace NET
} // namespace SL