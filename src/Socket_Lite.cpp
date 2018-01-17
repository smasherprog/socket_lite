
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
    sockaddr::sockaddr(unsigned char *buffer, int len, char *host, unsigned short port, Address_Family family)
    {
        assert(len < sizeof(SocketImpl));
        memcpy(SocketImpl, buffer, len);
        SocketImplLen = len;
        Host = host;
        Port = port;
        Family = family;
    }
    const unsigned char *sockaddr::get_SocketAddr() const { return SocketImpl; }
    int sockaddr::get_SocketAddrLen() const { return SocketImplLen; }
    std::string sockaddr::get_Host() const { return Host; }
    unsigned short sockaddr::get_Port() const { return htons(Port); }
    Address_Family sockaddr::get_Family() const { return Family; }

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
            std::cerr << "getaddrinfo" << WSAGetLastError() << std::endl;
            return ret;
        }
        for (auto ptr = result; ptr != NULL; ptr = ptr->ai_next) {

            char str[INET_ADDRSTRLEN];
            if (ptr->ai_family == AF_INET6 && family == Address_Family::IPV6) {
                auto so = (::sockaddr_in6 *)ptr->ai_addr;
                inet_ntop(AF_INET6, &so->sin6_addr, str, INET_ADDRSTRLEN);
                SL::NET::sockaddr tmp((unsigned char *)so, sizeof(sockaddr_in6), str, so->sin6_port, Address_Family::IPV6);
                ret.push_back(tmp);
            }
            else if (ptr->ai_family == AF_INET && family == Address_Family::IPV4) {
                auto so = (::sockaddr_in *)ptr->ai_addr;
                inet_ntop(AF_INET, &so->sin_addr, str, INET_ADDRSTRLEN);
                SL::NET::sockaddr tmp((unsigned char *)so, sizeof(sockaddr_in), str, so->sin_port, Address_Family::IPV4);
                ret.push_back(tmp);
            }
        }
        freeaddrinfo(result);
        return ret;
    } // namespace NET

    bool ISocket::listen(int backlog) const
    {
        if (handle != INVALID_SOCKET) {
            if (::listen(handle, backlog) == SOCKET_ERROR) {

                std::cerr << "listen error " << WSAGetLastError() << std::endl;
                return false;
            }
            return true;
        }
        return false;
    }
    ConnectionAttemptStatus ISocket::connect(SL::NET::sockaddr &addresses) const
    {
        if (::connect(handle, (::sockaddr *)addresses.get_SocketAddr(), addresses.get_SocketAddrLen()) == SOCKET_ERROR) {
            std::cerr << "connect error " << WSAGetLastError() << std::endl;
            return ConnectionAttemptStatus::FailedConnect;
        }
        return ConnectionAttemptStatus::SuccessfullConnect;
    }
    bool ISocket::bind(sockaddr addr)
    {
#if WIN32
        if (handle == INVALID_SOCKET) {
            if (addr.get_Family() == Address_Family::IPV4) {
                handle = WSASocketW(AF_INET, SOCK_STREAM, IPPROTO_IP, NULL, 0, WSA_FLAG_OVERLAPPED);
            }
            else {
                handle = WSASocketW(AF_INET6, SOCK_STREAM, IPPROTO_IP, NULL, 0, WSA_FLAG_OVERLAPPED);
            }
        }
#else
        if (handle == INVALID_SOCKET) {
            if (addr.Family == Address_Family::IPV4) {
                handle = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
            }
            else {
                handle = socket(AF_INET6, SOCK_STREAM, IPPROTO_TCP);
            }
        }

#endif

        if (::bind(handle, (::sockaddr *)addr.get_SocketAddr(), addr.get_SocketAddrLen()) == SOCKET_ERROR) {
            std::cerr << "bind error " << WSAGetLastError() << std::endl;
            return false;
        }
        return true;
    }

    std::optional<SL::NET::sockaddr> ISocket::getpeername() const
    {
        sockaddr_storage addr = {0};
        socklen_t len = sizeof(addr);

        char str[INET_ADDRSTRLEN];
        if (::getpeername(handle, (::sockaddr *)&addr, &len) == 0) {
            // deal with both IPv4 and IPv6:
            if (addr.ss_family == AF_INET) {
                auto so = (::sockaddr_in *)&addr;
                inet_ntop(AF_INET, &so->sin_addr, str, INET_ADDRSTRLEN);
                return std::optional<SL::NET::sockaddr>(
                    SL::NET::sockaddr((unsigned char *)so, sizeof(sockaddr_in), str, so->sin_port, Address_Family::IPV4));
            }
            else { // AF_INET6
                auto so = (sockaddr_in6 *)&addr;
                inet_ntop(AF_INET6, &so->sin6_addr, str, INET_ADDRSTRLEN);
                return std::optional<SL::NET::sockaddr>(
                    SL::NET::sockaddr((unsigned char *)so, sizeof(sockaddr_in6), str, so->sin6_port, Address_Family::IPV6));
            }
        }
        return std::nullopt;
    }
    std::optional<SL::NET::sockaddr> ISocket::getsockname() const
    {
        sockaddr_storage addr = {0};
        socklen_t len = sizeof(addr);
        char str[INET_ADDRSTRLEN];
        if (::getsockname(handle, (::sockaddr *)&addr, &len) == 0) {
            // deal with both IPv4 and IPv6:
            if (addr.ss_family == AF_INET) {
                auto so = (::sockaddr_in *)&addr;
                inet_ntop(AF_INET, &so->sin_addr, str, INET_ADDRSTRLEN);
                return std::optional<SL::NET::sockaddr>(
                    SL::NET::sockaddr((unsigned char *)so, sizeof(sockaddr_in), str, so->sin_port, Address_Family::IPV4));
            }
            else { // AF_INET6
                auto so = (::sockaddr_in6 *)&addr;
                inet_ntop(AF_INET6, &so->sin6_addr, str, INET_ADDRSTRLEN);
                return std::optional<SL::NET::sockaddr>(
                    SL::NET::sockaddr((unsigned char *)so, sizeof(sockaddr_in6), str, so->sin6_port, Address_Family::IPV6));
            }
        }
        return std::nullopt;
    }
    int ISocket::recv(int buffer_size, unsigned char *buffer, RecvFlags flags)
    {
        int f = RecvFlags::OOB & flags ? MSG_OOB : 0;
        f = RecvFlags::PEEK & flags ? MSG_PEEK | f : f;
        f = RecvFlags::WAITALL & flags ? MSG_WAITALL | f : f;

        auto bytesread = ::recv(handle, (char *)buffer, buffer_size, f);
        if (bytesread > 0) {
            return bytesread;
        }
        else if (auto wsaerr = WSAGetLastError(); bytesread == 0 || (bytesread == SOCKET_ERROR && wsaerr != WSAEWOULDBLOCK)) {
            std::cerr << "recv() failed: " << wsaerr << std::endl;
            return -1;
        }
        return 0;
    }
    std::optional<bool> ISocket::getsockopt_O_DEBUG() const
    {
        int value = 0;
        int valuelen = sizeof(value);
        if (::getsockopt(handle, SOL_SOCKET, SO_DEBUG, (char *)&value, &valuelen) == 0) {
            return std::optional<bool>(value != 0);
        }
        return std::nullopt;
    }

    std::optional<bool> ISocket::getsockopt_O_ACCEPTCONN() const
    {
        int value = 0;
        int valuelen = sizeof(value);
        if (::getsockopt(handle, SOL_SOCKET, SO_ACCEPTCONN, (char *)&value, &valuelen) == 0) {
            return std::optional<bool>(value != 0);
        }
        return std::nullopt;
    }

    std::optional<bool> ISocket::getsockopt_O_BROADCAST() const
    {
        int value = 0;
        int valuelen = sizeof(value);
        if (::getsockopt(handle, SOL_SOCKET, SO_BROADCAST, (char *)&value, &valuelen) == 0) {
            return std::optional<bool>(value != 0);
        }
        return std::nullopt;
    }

    std::optional<bool> ISocket::getsockopt_O_REUSEADDR() const
    {
        int value = 0;
        int valuelen = sizeof(value);
        if (::getsockopt(handle, SOL_SOCKET, SO_REUSEADDR, (char *)&value, &valuelen) == 0) {
            return std::optional<bool>(value != 0);
        }
        return std::nullopt;
    }

    std::optional<bool> ISocket::getsockopt_O_KEEPALIVE() const
    {
        int value = 0;
        int valuelen = sizeof(value);
        if (::getsockopt(handle, SOL_SOCKET, SO_KEEPALIVE, (char *)&value, &valuelen) == 0) {
            return std::optional<bool>(value != 0);
        }
        return std::nullopt;
    }

    std::optional<Linger_Option> ISocket::getsockopt_O_LINGER() const
    {
        linger value;
        int valuelen = sizeof(value);
        if (::getsockopt(handle, SOL_SOCKET, SO_LINGER, (char *)&value, &valuelen) == 0) {
            return std::optional<Linger_Option>(
                {value.l_onoff == 0 ? Linger_Options::LINGER_OFF : Linger_Options::LINGER_ON, std::chrono::seconds(value.l_linger)});
        }
        return std::nullopt;
    }

    std::optional<bool> ISocket::getsockopt_O_OOBINLINE() const
    {
        int value = 0;
        int valuelen = sizeof(value);
        if (::getsockopt(handle, SOL_SOCKET, SO_OOBINLINE, (char *)&value, &valuelen) == 0) {
            return std::optional<bool>(value != 0);
        }
        return std::nullopt;
    }

    std::optional<bool> ISocket::getsockopt_O_EXCLUSIVEADDRUSE() const
    {
        int value = 0;
        int valuelen = sizeof(value);
        if (::getsockopt(handle, SOL_SOCKET, SO_EXCLUSIVEADDRUSE, (char *)&value, &valuelen) == 0) {
            return std::optional<bool>(value != 0);
        }
        return std::nullopt;
    }

    std::optional<int> ISocket::getsockopt_O_SNDBUF() const
    {
        int value = 0;
        int valuelen = sizeof(value);
        if (::getsockopt(handle, SOL_SOCKET, SO_SNDBUF, (char *)&value, &valuelen) == 0) {
            return std::optional<int>(value);
        }
        return std::nullopt;
    }

    std::optional<int> ISocket::getsockopt_O_RCVBUF() const
    {
        int value = 0;
        int valuelen = sizeof(value);
        if (::getsockopt(handle, SOL_SOCKET, SO_RCVBUF, (char *)&value, &valuelen) == 0) {
            return std::optional<int>(value);
        }
        return std::nullopt;
    }

    std::optional<std::chrono::seconds> ISocket::getsockopt_O_SNDTIMEO() const
    {
#ifdef WIN32
        DWORD value = 0;
        int valuelen = sizeof(value);
        if (::getsockopt(handle, SOL_SOCKET, SO_SNDTIMEO, (char *)&value, &valuelen) == 0) {
            return std::optional<std::chrono::seconds>(value * 1000); // convert from ms to seconds
        }
#else
        timeval value = {0};
        int valuelen = sizeof(value);
        if (::getsockopt(handle, SOL_SOCKET, SO_SNDTIMEO, (char *)&value, &valuelen) == 0) {
            return std::optional<std::chrono::seconds>(value.tv_sec); // convert from ms to seconds
        }
#endif
        return std::nullopt;
    }

    std::optional<std::chrono::seconds> ISocket::getsockopt_O_RCVTIMEO() const
    {
#ifdef WIN32
        DWORD value = 0;
        int valuelen = sizeof(value);
        if (::getsockopt(handle, SOL_SOCKET, SO_RCVTIMEO, (char *)&value, &valuelen) == 0) {
            return std::optional<std::chrono::seconds>(value * 1000); // convert from ms to seconds
        }
#else
        timeval value = {0};
        int valuelen = sizeof(value);
        if (::getsockopt(handle, SOL_SOCKET, SO_RCVTIMEO, (char *)&value, &valuelen) == 0) {
            return std::optional<std::chrono::seconds>(value.tv_sec); // convert from ms to seconds
        }
#endif
        return std::nullopt;
    }

    std::optional<int> ISocket::getsockopt_O_ERROR() const
    {
        int value = 0;
        int valuelen = sizeof(value);
        if (::getsockopt(handle, SOL_SOCKET, SO_ERROR, (char *)&value, &valuelen) == 0) {
            return std::optional<int>(value);
        }
        return std::nullopt;
    }

    std::optional<bool> ISocket::getsockopt_O_NODELAY() const
    {
        int value = 0;
        int valuelen = sizeof(value);
        if (::getsockopt(handle, IPPROTO_TCP, TCP_NODELAY, (char *)&value, &valuelen) == 0) {
            return std::optional<bool>(value != 0);
        }
        return std::nullopt;
    }

    std::optional<Blocking_Options> ISocket::getsockopt_O_BLOCKING() const
    {
#ifdef WIN32
        // not supported on windows
        return std::nullopt;
#else
        long arg = 0;
        if ((arg = fcntl(handle, F_GETFL, NULL)) < 0) {
            return std::nullopt;
        }
        return std::optional<Blocking_Options>((arg & O_NONBLOCK) != 0 ? Blocking_Options::NON_BLOCKING : Blocking_Options::BLOCKING);
#endif
    }

    bool ISocket::setsockopt_O_DEBUG(bool b) const
    {
        int value = b ? 1 : 0;
        int valuelen = sizeof(value);
        return ::setsockopt(handle, SOL_SOCKET, SO_DEBUG, (char *)&value, valuelen) == 0;
    }

    bool ISocket::setsockopt_O_BROADCAST(bool b) const
    {
        int value = b ? 1 : 0;
        int valuelen = sizeof(value);
        return ::setsockopt(handle, SOL_SOCKET, SO_BROADCAST, (char *)&value, valuelen) == 0;
    }

    bool ISocket::setsockopt_O_REUSEADDR(bool b) const
    {
        int value = b ? 1 : 0;
        int valuelen = sizeof(value);
        return ::setsockopt(handle, SOL_SOCKET, SO_BROADCAST, (char *)&value, valuelen) == 0;
    }

    bool ISocket::setsockopt_O_KEEPALIVE(bool b) const
    {
        int value = b ? 1 : 0;
        int valuelen = sizeof(value);
        return ::setsockopt(handle, SOL_SOCKET, SO_KEEPALIVE, (char *)&value, valuelen) == 0;
    }

    bool ISocket::setsockopt_O_LINGER(Linger_Option o) const
    {
        linger value;
        value.l_onoff = o.l_onoff == Linger_Options::LINGER_OFF ? 0 : 1;
        value.l_linger = static_cast<unsigned short>(o.l_linger.count());
        int valuelen = sizeof(value);
        return ::setsockopt(handle, SOL_SOCKET, SO_LINGER, (char *)&value, valuelen) == 0;
    }

    bool ISocket::setsockopt_O_OOBINLINE(bool b) const
    {
        int value = b ? 1 : 0;
        int valuelen = sizeof(value);
        return ::setsockopt(handle, SOL_SOCKET, SO_OOBINLINE, (char *)&value, valuelen) == 0;
    }

    bool ISocket::setsockopt_O_EXCLUSIVEADDRUSE(bool b) const
    {
        int value = b ? 1 : 0;
        int valuelen = sizeof(value);
        return ::setsockopt(handle, SOL_SOCKET, SO_EXCLUSIVEADDRUSE, (char *)&value, valuelen) == 0;
    }

    bool ISocket::setsockopt_O_SNDBUF(int b) const
    {
        int value = b;
        int valuelen = sizeof(value);
        return ::setsockopt(handle, SOL_SOCKET, SO_SNDBUF, (char *)&value, valuelen) == 0;
    }

    bool ISocket::setsockopt_O_RCVBUF(int b) const
    {
        int value = b;
        int valuelen = sizeof(value);
        return ::setsockopt(handle, SOL_SOCKET, SO_RCVBUF, (char *)&value, valuelen) == 0;
    }

    bool ISocket::setsockopt_O_SNDTIMEO(std::chrono::seconds sec) const
    {
#ifdef WIN32
        DWORD value = static_cast<DWORD>(sec.count() * 1000); // convert to miliseconds for windows
        int valuelen = sizeof(value);
        return ::setsockopt(handle, SOL_SOCKET, SO_SNDTIMEO, (char *)&value, valuelen) == 0;
#else
        timeval tv = {0};
        tv.tv_sec = static_cast<long>(sec.count());
        return ::setsockopt(handle, SOL_SOCKET, SO_SNDTIMEO, (timeval *)&tv, sizeof(tv)) == 0;
#endif
    }

    bool ISocket::setsockopt_O_RCVTIMEO(std::chrono::seconds sec) const
    {
#ifdef WIN32
        DWORD value = static_cast<DWORD>(sec.count() * 1000); // convert to miliseconds for windows
        int valuelen = sizeof(value);
        return ::setsockopt(handle, SOL_SOCKET, SO_RCVTIMEO, (char *)&value, valuelen) == 0;
#else
        timeval tv = {0};
        tv.tv_sec = static_cast<long>(sec.count());
        return ::setsockopt(handle, SOL_SOCKET, SO_RCVTIMEO, (timeval *)&tv, sizeof(tv)) == 0;
#endif
    }

    bool ISocket::setsockopt_O_NODELAY(bool b) const
    {
        int value = b ? 1 : 0;
        int valuelen = sizeof(value);
        return ::setsockopt(handle, IPPROTO_TCP, TCP_NODELAY, (char *)&value, valuelen) == 0;
    }

    bool ISocket::setsockopt_O_BLOCKING(Blocking_Options b) const
    {

#ifdef WIN32
        u_long iMode = b == Blocking_Options::BLOCKING ? 0 : 1;
        if (auto nRet = ioctlsocket(handle, FIONBIO, &iMode); nRet != NO_ERROR) {
            return false;
        }
#else
        int flags = fcntl(handle, F_GETFL, 0);
        if (flags == -1)
            return false;
        flags = b == Blocking_Options::BLOCKING ? (flags & ~O_NONBLOCK) : (flags | O_NONBLOCK);
        return (fcntl(handle, F_SETFL, flags) == 0) ? true : false;
    }
#endif
        return true;
    } // namespace NET

} // namespace NET
} // namespace SL