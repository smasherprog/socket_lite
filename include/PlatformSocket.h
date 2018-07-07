#pragma once
#include "defs.h"

namespace SL {
namespace NET {

    class PlatformSocket {
        SocketHandle Handle_;

      public:
        PlatformSocket() : Handle_(INVALID_SOCKET) {}
        PlatformSocket(SocketHandle h) : Handle_(h) {}
        PlatformSocket(const AddressFamily &family, Blocking_Options opts) : Handle_(INVALID_SOCKET)
        {
            int typ = SOCK_STREAM;

#if !_WIN32
            if (opts == Blocking_Options::NON_BLOCKING) {

                typ |= SOCK_NONBLOCK;
            }
#endif
            if (family == AddressFamily::IPV4) {
                Handle_.value = socket(AF_INET, typ, 0);
            }
            else {
                Handle_.value = socket(AF_INET6, typ, 0);
            }
#if _WIN32

            if (opts == Blocking_Options::NON_BLOCKING) {
                [[maybe_unused]] auto e = setsockopt(BLOCKINGTag{}, opts);
            }
#endif
        }
        ~PlatformSocket() { close(); }
        PlatformSocket(PlatformSocket &&p) : Handle_(std::move(p.Handle_)) { p.Handle_.value = INVALID_SOCKET; }

        PlatformSocket &operator=(PlatformSocket &&p)
        {
            close();
            Handle_.value = p.Handle_.value;
            p.Handle_.value = INVALID_SOCKET;
            return *this;
        }
        [[nodiscard]] SocketHandle Handle() const { return Handle_; }
        operator bool() const { return Handle_.value != INVALID_SOCKET; }
        void close()
        {
            auto t = Handle_.value;
            Handle_.value = INVALID_SOCKET;
            if (t != INVALID_SOCKET) {

#ifdef _WIN32
                closesocket(t);
#else
                ::close(t);
#endif
            }
        }
        void shutdown(ShutDownOptions o)
        {
            auto t = Handle_.value;
            if (t != INVALID_SOCKET) {
                switch (o) {
#if _WIN32

                case ShutDownOptions::SHUTDOWN_READ:
                    ::shutdown(t, SD_RECEIVE);
                    return;

                case ShutDownOptions::SHUTDOWN_WRITE:
                    ::shutdown(t, SD_SEND);
                    return;

                case ShutDownOptions::SHUTDOWN_BOTH:
                    ::shutdown(t, SD_BOTH);
                    return;
#else

                case ShutDownOptions::SHUTDOWN_READ:
                    ::shutdown(t, SHUT_RD);
                    return;

                case ShutDownOptions::SHUTDOWN_WRITE:
                    ::shutdown(t, SHUT_WR);
                    return;

                case ShutDownOptions::SHUTDOWN_BOTH:
                    ::shutdown(t, SHUT_RDWR);
                    return;
#endif
                default:
                    return;
                }
            }
        }

        std::tuple<StatusCode, int> send(unsigned char *buf, int len, int flags)
        {
#if _WIN32
            auto count = ::send(Handle_.value, (char *)buf, static_cast<int>(len), flags);
            if (count < 0) { // possible error or continue
                if (auto er = WSAGetLastError(); er != WSAEWOULDBLOCK) {
                    return std::tuple(TranslateError(&er), 0);
                }
                else {
                    return std::tuple(StatusCode::SC_EWOULDBLOCK, 0);
                }
            }
            else {
                return std::tuple(StatusCode::SC_SUCCESS, count);
            }
#else
            auto count = ::send(Handle_.value, buf, static_cast<int>(len), flags | MSG_NOSIGNAL);
            if (count < 0) { // possible error or continue
                if (errno != EAGAIN && errno != EINTR) {
                    return std::tuple(TranslateError(), 0);
                }
                else {
                    return std::tuple(StatusCode::SC_EWOULDBLOCK, 0);
                }
            }
            else {
                return std::tuple(StatusCode::SC_SUCCESS, count);
            }
#endif
        }
        std::tuple<StatusCode, int> recv(unsigned char *buf, int len, int flags)
        {
#if _WIN32
            auto count = ::recv(Handle_.value, (char *)buf, static_cast<int>(len), flags);
            if (count <= 0) { // possible error or continue
                if (auto er = WSAGetLastError(); er != WSAEWOULDBLOCK) {
                    return std::tuple(TranslateError(&er), 0);
                }
                else {
                    return std::tuple(StatusCode::SC_EWOULDBLOCK, 0);
                }
            }
            else {
                return std::tuple(StatusCode::SC_SUCCESS, count);
            }
#else
            auto count = ::recv(Handle_.value, buf, static_cast<int>(len), flags | MSG_NOSIGNAL);
            if (count <= 0) { // possible error or continue
                if ((errno != EAGAIN && errno != EINTR) || count == 0) {
                    return std::tuple(TranslateError(), 0);
                }
                else {
                    return std::tuple(StatusCode::SC_EWOULDBLOCK, 0);
                }
            }
            else {
                return std::tuple(StatusCode::SC_SUCCESS, count);
            }
#endif
        }

        StatusCode bind(const SocketAddress &addr)
        {
            if (::bind(Handle_.value, (::sockaddr *)SocketAddr(addr), SocketAddrLen(addr)) == SOCKET_ERROR) {
                return TranslateError();
            }
            return StatusCode::SC_SUCCESS;
        }
        StatusCode listen(int backlog)
        {
            if (::listen(Handle_.value, backlog) == SOCKET_ERROR) {
                return TranslateError();
            }
            return StatusCode::SC_SUCCESS;
        }
        StatusCode getpeername(const std::function<void(const SocketAddress &)> &callback) const
        {
            sockaddr_storage addr = {0};
            socklen_t len = sizeof(addr);

            char str[INET_ADDRSTRLEN];
            if (::getpeername(Handle_.value, (::sockaddr *)&addr, &len) == 0) {
                // deal with both IPv4 and IPv6:
                if (addr.ss_family == AF_INET) {
                    auto so = (::sockaddr_in *)&addr;
                    inet_ntop(AF_INET, &so->sin_addr, str, INET_ADDRSTRLEN);
                    callback(SL::NET::SocketAddress((unsigned char *)so, sizeof(sockaddr_in), str, so->sin_port, AddressFamily::IPV4));
                }
                else { // AF_INET6
                    auto so = (sockaddr_in6 *)&addr;
                    inet_ntop(AF_INET6, &so->sin6_addr, str, INET_ADDRSTRLEN);
                    callback(SL::NET::SocketAddress((unsigned char *)so, sizeof(sockaddr_in6), str, so->sin6_port, AddressFamily::IPV6));
                    return StatusCode::SC_SUCCESS;
                }
            }
            return TranslateError();
        }
        StatusCode getsockname(const std::function<void(const SocketAddress &)> &callback) const
        {
            sockaddr_storage addr = {0};
            socklen_t len = sizeof(addr);
            char str[INET_ADDRSTRLEN];
            if (::getsockname(Handle_.value, (::sockaddr *)&addr, &len) == 0) {
                // deal with both IPv4 and IPv6:
                if (addr.ss_family == AF_INET) {
                    auto so = (::sockaddr_in *)&addr;
                    inet_ntop(AF_INET, &so->sin_addr, str, INET_ADDRSTRLEN);
                    callback(SL::NET::SocketAddress((unsigned char *)so, sizeof(sockaddr_in), str, so->sin_port, AddressFamily::IPV4));
                }
                else { // AF_INET6
                    auto so = (::sockaddr_in6 *)&addr;
                    inet_ntop(AF_INET6, &so->sin6_addr, str, INET_ADDRSTRLEN);
                    callback(SL::NET::SocketAddress((unsigned char *)so, sizeof(sockaddr_in6), str, so->sin6_port, AddressFamily::IPV6));
                }
                return StatusCode::SC_SUCCESS;
            }
            return TranslateError();
        }

        StatusCode getsockopt(DEBUGTag, const std::function<void(const SockOptStatus &)> &callback) const
        {
            int value = 0;
            SOCKLEN_T valuelen = sizeof(value);
            if (::getsockopt(Handle_.value, SOL_SOCKET, SO_DEBUG, (char *)&value, &valuelen) == 0) {
                callback(value != 0 ? SockOptStatus::ENABLED : SockOptStatus::DISABLED);
                return StatusCode::SC_SUCCESS;
            }
            return TranslateError();
        }

        StatusCode getsockopt(ACCEPTCONNTag, const std::function<void(const SockOptStatus &)> &callback) const
        {
            int value = 0;
            SOCKLEN_T valuelen = sizeof(value);
            if (::getsockopt(Handle_.value, SOL_SOCKET, SO_ACCEPTCONN, (char *)&value, &valuelen) == 0) {
                callback(value != 0 ? SockOptStatus::ENABLED : SockOptStatus::DISABLED);
                return StatusCode::SC_SUCCESS;
            }
            return TranslateError();
        }

        StatusCode getsockopt(BROADCASTTag, const std::function<void(const SockOptStatus &)> &callback) const
        {
            int value = 0;
            SOCKLEN_T valuelen = sizeof(value);
            if (::getsockopt(Handle_.value, SOL_SOCKET, SO_BROADCAST, (char *)&value, &valuelen) == 0) {
                callback(value != 0 ? SockOptStatus::ENABLED : SockOptStatus::DISABLED);
                return StatusCode::SC_SUCCESS;
            }
            return TranslateError();
        }

        StatusCode getsockopt(REUSEADDRTag, const std::function<void(const SockOptStatus &)> &callback) const
        {
            int value = 0;
            SOCKLEN_T valuelen = sizeof(value);
            if (::getsockopt(Handle_.value, SOL_SOCKET, SO_REUSEADDR, (char *)&value, &valuelen) == 0) {
                callback(value != 0 ? SockOptStatus::ENABLED : SockOptStatus::DISABLED);
                return StatusCode::SC_SUCCESS;
            }
            return TranslateError();
        }

        StatusCode getsockopt(KEEPALIVETag, const std::function<void(const SockOptStatus &)> &callback) const
        {
            int value = 0;
            SOCKLEN_T valuelen = sizeof(value);
            if (::getsockopt(Handle_.value, SOL_SOCKET, SO_KEEPALIVE, (char *)&value, &valuelen) == 0) {
                callback(value != 0 ? SockOptStatus::ENABLED : SockOptStatus::DISABLED);
                return StatusCode::SC_SUCCESS;
            }
            return TranslateError();
        }

        StatusCode getsockopt(LINGERTag, const std::function<void(const LingerOption &)> &callback) const
        {
            linger value;
            SOCKLEN_T valuelen = sizeof(value);
            if (::getsockopt(Handle_.value, SOL_SOCKET, SO_LINGER, (char *)&value, &valuelen) == 0) {
                callback(
                    LingerOption{value.l_onoff == 0 ? LingerOptions::LINGER_OFF : LingerOptions::LINGER_ON, std::chrono::seconds(value.l_linger)});
                return StatusCode::SC_SUCCESS;
            }
            return TranslateError();
        }

        StatusCode getsockopt(OOBINLINETag, const std::function<void(const SockOptStatus &)> &callback) const
        {
            int value = 0;
            SOCKLEN_T valuelen = sizeof(value);
            if (::getsockopt(Handle_.value, SOL_SOCKET, SO_OOBINLINE, (char *)&value, &valuelen) == 0) {
                callback(value != 0 ? SockOptStatus::ENABLED : SockOptStatus::DISABLED);
                return StatusCode::SC_SUCCESS;
            }
            return TranslateError();
        }

        StatusCode getsockopt(SNDBUFTag, const std::function<void(const int &)> &callback) const
        {
            int value = 0;
            SOCKLEN_T valuelen = sizeof(value);
            if (::getsockopt(Handle_.value, SOL_SOCKET, SO_SNDBUF, (char *)&value, &valuelen) == 0) {
                callback(value);
                return StatusCode::SC_SUCCESS;
            }
            return TranslateError();
        }

        StatusCode getsockopt(RCVBUFTag, const std::function<void(const int &)> &callback) const
        {
            int value = 0;
            SOCKLEN_T valuelen = sizeof(value);
            if (::getsockopt(Handle_.value, SOL_SOCKET, SO_RCVBUF, (char *)&value, &valuelen) == 0) {
                callback(value);
                return StatusCode::SC_SUCCESS;
            }
            return TranslateError();
        }

        StatusCode getsockopt(SNDTIMEOTag, const std::function<void(const std::chrono::seconds &)> &callback) const
        {
#ifdef _WIN32
            DWORD value = 0;
            int valuelen = sizeof(value);
            if (::getsockopt(Handle_.value, SOL_SOCKET, SO_SNDTIMEO, (char *)&value, &valuelen) == 0) {
                callback(std::chrono::seconds(value * 1000)); // convert from ms to seconds
                return StatusCode::SC_SUCCESS;
            }
#else
            timeval value = {0};
            SOCKLEN_T valuelen = sizeof(value);
            if (::getsockopt(Handle_.value, SOL_SOCKET, SO_SNDTIMEO, (char *)&value, &valuelen) == 0) {
                callback(std::chrono::seconds(value.tv_sec)); // convert from ms to seconds
                return StatusCode::SC_SUCCESS;
            }
#endif
            return TranslateError();
        }

        StatusCode getsockopt(RCVTIMEOTag, const std::function<void(const std::chrono::seconds &)> &callback) const
        {
#ifdef _WIN32
            DWORD value = 0;
            int valuelen = sizeof(value);
            if (::getsockopt(Handle_.value, SOL_SOCKET, SO_RCVTIMEO, (char *)&value, &valuelen) == 0) {
                callback(std::chrono::seconds(value * 1000)); // convert from ms to seconds
                return StatusCode::SC_SUCCESS;
            }
#else
            timeval value = {0};
            SOCKLEN_T valuelen = sizeof(value);
            if (::getsockopt(Handle_.value, SOL_SOCKET, SO_SNDTIMEO, (char *)&value, &valuelen) == 0) {
                callback(std::chrono::seconds(value.tv_sec)); // convert from ms to seconds
                return StatusCode::SC_SUCCESS;
            }
#endif
            return TranslateError();
        }

        StatusCode getsockopt(ERRORTag, const std::function<void(const int &)> &callback) const
        {
            int value = 0;
            SOCKLEN_T valuelen = sizeof(value);
            if (::getsockopt(Handle_.value, SOL_SOCKET, SO_ERROR, (char *)&value, &valuelen) == 0) {
                callback(value);
                return StatusCode::SC_SUCCESS;
            }
            return TranslateError();
        }

        StatusCode getsockopt(NODELAYTag, const std::function<void(const SockOptStatus &)> &callback) const
        {
            int value = 0;
            SOCKLEN_T valuelen = sizeof(value);
            if (::getsockopt(Handle_.value, IPPROTO_TCP, TCP_NODELAY, (char *)&value, &valuelen) == 0) {
                callback(value != 0 ? SockOptStatus::ENABLED : SockOptStatus::DISABLED);
                return StatusCode::SC_SUCCESS;
            }
            return TranslateError();
        }
#ifdef _WIN32
        StatusCode getsockopt(BLOCKINGTag, const std::function<void(const Blocking_Options &)> &) const
        {
            return StatusCode::SC_NOTSUPPORTED;
#else
        StatusCode getsockopt(BLOCKINGTag, const std::function<void(const Blocking_Options &)> &callback) const
        {
            long arg = 0;
            if ((arg = fcntl(Handle_.value, F_GETFL, NULL)) < 0) {
                return TranslateError();
            }
            callback((arg & O_NONBLOCK) != 0 ? Blocking_Options::NON_BLOCKING : Blocking_Options::BLOCKING);
            return StatusCode::SC_SUCCESS;
#endif
        }
        StatusCode setsockopt(DEBUGTag, SockOptStatus b)
        {
            int value = b == SockOptStatus::ENABLED ? 1 : 0;
            int valuelen = sizeof(value);
            if (::setsockopt(Handle_.value, SOL_SOCKET, SO_DEBUG, (char *)&value, valuelen) == 0) {
                return StatusCode::SC_SUCCESS;
            }
            return TranslateError();
        }

        StatusCode setsockopt(BROADCASTTag, SockOptStatus b)
        {
            int value = b == SockOptStatus::ENABLED ? 1 : 0;
            int valuelen = sizeof(value);
            if (::setsockopt(Handle_.value, SOL_SOCKET, SO_BROADCAST, (char *)&value, valuelen) == 0) {
                return StatusCode::SC_SUCCESS;
            }
            return TranslateError();
        }

        StatusCode setsockopt(REUSEADDRTag, SockOptStatus b)
        {
            int value = b == SockOptStatus::ENABLED ? 1 : 0;
            int valuelen = sizeof(value);
            if (::setsockopt(Handle_.value, SOL_SOCKET, SO_BROADCAST, (char *)&value, valuelen) == 0) {
                return StatusCode::SC_SUCCESS;
            }
            return TranslateError();
        }

        StatusCode setsockopt(KEEPALIVETag, SockOptStatus b)
        {
            int value = b == SockOptStatus::ENABLED ? 1 : 0;
            int valuelen = sizeof(value);
            if (::setsockopt(Handle_.value, SOL_SOCKET, SO_KEEPALIVE, (char *)&value, valuelen) == 0) {
                return StatusCode::SC_SUCCESS;
            }
            return TranslateError();
        }

        StatusCode setsockopt(LINGERTag, LingerOption o)
        {
            linger value;
            value.l_onoff = o.l_onoff == LingerOptions::LINGER_OFF ? 0 : 1;
            value.l_linger = static_cast<unsigned short>(o.l_linger.count());
            int valuelen = sizeof(value);
            if (::setsockopt(Handle_.value, SOL_SOCKET, SO_LINGER, (char *)&value, valuelen) == 0) {
                return StatusCode::SC_SUCCESS;
            }
            return TranslateError();
        }

        StatusCode setsockopt(OOBINLINETag, SockOptStatus b)
        {
            int value = b == SockOptStatus::ENABLED ? 1 : 0;
            int valuelen = sizeof(value);
            if (::setsockopt(Handle_.value, SOL_SOCKET, SO_OOBINLINE, (char *)&value, valuelen) == 0) {
                return StatusCode::SC_SUCCESS;
            }
            return TranslateError();
        }

        StatusCode setsockopt(SNDBUFTag, int b)
        {
            int value = b;
            int valuelen = sizeof(value);
            if (::setsockopt(Handle_.value, SOL_SOCKET, SO_SNDBUF, (char *)&value, valuelen) == 0) {
                return StatusCode::SC_SUCCESS;
            }
            return TranslateError();
        }

        StatusCode setsockopt(RCVBUFTag, int b)
        {
            int value = b;
            int valuelen = sizeof(value);
            if (::setsockopt(Handle_.value, SOL_SOCKET, SO_RCVBUF, (char *)&value, valuelen) == 0) {
                return StatusCode::SC_SUCCESS;
            }
            return TranslateError();
        }

        StatusCode setsockopt(SNDTIMEOTag, std::chrono::seconds sec)
        {
#ifdef _WIN32
            DWORD value = static_cast<DWORD>(sec.count() * 1000); // convert to miliseconds for windows
            int valuelen = sizeof(value);
            if (::setsockopt(Handle_.value, SOL_SOCKET, SO_SNDTIMEO, (char *)&value, valuelen) == 0
#else
            timeval tv = {0};
            tv.tv_sec = static_cast<long>(sec.count());
            if (::setsockopt(Handle_.value, SOL_SOCKET, SO_SNDTIMEO, (timeval *)&tv, sizeof(tv)) == 0
#endif
            ) {
                return StatusCode::SC_SUCCESS;
            }
            return TranslateError();
        }

        StatusCode setsockopt(RCVTIMEOTag, std::chrono::seconds sec)
        {

#ifdef WIN32
            DWORD value = static_cast<DWORD>(sec.count() * 1000); // convert to miliseconds for windows
            int valuelen = sizeof(value);
            if (::setsockopt(Handle_.value, SOL_SOCKET, SO_RCVTIMEO, (char *)&value, valuelen) == 0
#else
            timeval tv = {0};
            tv.tv_sec = static_cast<long>(sec.count());
            if (::setsockopt(Handle_.value, SOL_SOCKET, SO_RCVTIMEO, (timeval *)&tv, sizeof(tv)) == 0
#endif
            ) {
                return StatusCode::SC_SUCCESS;
            }
            return TranslateError();
        }

        StatusCode setsockopt(NODELAYTag, SockOptStatus b)
        {
            int value = b == SockOptStatus::ENABLED ? 1 : 0;
            int valuelen = sizeof(value);
            if (::setsockopt(Handle_.value, IPPROTO_TCP, TCP_NODELAY, (char *)&value, valuelen) == 0) {
                return StatusCode::SC_SUCCESS;
            }
            return TranslateError();
        }

        StatusCode setsockopt(BLOCKINGTag, Blocking_Options b)
        {

#ifdef WIN32
            u_long iMode = b == Blocking_Options::BLOCKING ? 0 : 1;
            if (auto nRet = ioctlsocket(Handle_.value, FIONBIO, &iMode); nRet != NO_ERROR) {
                return TranslateError();
            }
            return StatusCode::SC_SUCCESS;
#else
            int flags = fcntl(Handle_.value, F_GETFL, 0);
            if (flags == 0)
                return TranslateError();
            flags = b == Blocking_Options::BLOCKING ? (flags & ~O_NONBLOCK) : (flags | O_NONBLOCK);
            return (fcntl(Handle_.value, F_SETFL, flags) == 0) ? StatusCode::SC_SUCCESS : TranslateError();
#endif
        }
    };

    [[nodiscard]] static std::vector<SocketAddress> getaddrinfo(const char *nodename, PortNumber pServiceName,
                                                               AddressFamily family = AddressFamily::IPANY)
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

} // namespace NET
} // namespace SL
