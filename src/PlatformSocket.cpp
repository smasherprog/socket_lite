
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

    PlatformSocket::PlatformSocket() : Handle_(INVALID_SOCKET) {}
    PlatformSocket::PlatformSocket(SocketHandle h) : Handle_(h) {}
    PlatformSocket::PlatformSocket(const AddressFamily &family, Blocking_Options opts) : Handle_(INVALID_SOCKET)
    {
        int typ = SOCK_STREAM;
        
#if !_WIN32
        if(opts == Blocking_Options::NON_BLOCKING){
            
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
        
        if(opts == Blocking_Options::NON_BLOCKING){
            setsockopt(BLOCKINGTag{}, opts);
        }
        #endif
    }
    PlatformSocket::~PlatformSocket() { close(); }
    PlatformSocket::PlatformSocket(PlatformSocket &&p) : Handle_(p.Handle_) { p.Handle_.value = INVALID_SOCKET; }

    PlatformSocket &PlatformSocket::operator=(PlatformSocket &&p)
    {
        close();
        Handle_.value = p.Handle_.value;
        p.Handle_.value = INVALID_SOCKET;
        return *this;
    }

    PlatformSocket::operator bool() const { return Handle_.value != INVALID_SOCKET; }
    void PlatformSocket::close()
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

    StatusCode PlatformSocket::bind(const sockaddr &addr)
    {
        if (::bind(Handle_.value, (::sockaddr *)SocketAddr(addr), SocketAddrLen(addr)) == SOCKET_ERROR) {
            return TranslateError();
        }
        return StatusCode::SC_SUCCESS;
    }
    StatusCode PlatformSocket::listen(int backlog)
    {
        if (::listen(Handle_.value, backlog) == SOCKET_ERROR) {
            return TranslateError();
        }
        return StatusCode::SC_SUCCESS;
    }
    StatusCode PlatformSocket::getpeername(const std::function<void(const sockaddr &)> &callback) const
    {
        sockaddr_storage addr = {0};
        socklen_t len = sizeof(addr);

        char str[INET_ADDRSTRLEN];
        if (::getpeername(Handle_.value, (::sockaddr *)&addr, &len) == 0) {
            // deal with both IPv4 and IPv6:
            if (addr.ss_family == AF_INET) {
                auto so = (::sockaddr_in *)&addr;
                inet_ntop(AF_INET, &so->sin_addr, str, INET_ADDRSTRLEN);
                callback(SL::NET::sockaddr((unsigned char *)so, sizeof(sockaddr_in), str, so->sin_port, AddressFamily::IPV4));
            }
            else { // AF_INET6
                auto so = (sockaddr_in6 *)&addr;
                inet_ntop(AF_INET6, &so->sin6_addr, str, INET_ADDRSTRLEN);
                callback(SL::NET::sockaddr((unsigned char *)so, sizeof(sockaddr_in6), str, so->sin6_port, AddressFamily::IPV6));
                return StatusCode::SC_SUCCESS;
            }
        }
        return TranslateError();
    }
    StatusCode PlatformSocket::getsockname(const std::function<void(const sockaddr &)> &callback) const
    {
        sockaddr_storage addr = {0};
        socklen_t len = sizeof(addr);
        char str[INET_ADDRSTRLEN];
        if (::getsockname(Handle_.value, (::sockaddr *)&addr, &len) == 0) {
            // deal with both IPv4 and IPv6:
            if (addr.ss_family == AF_INET) {
                auto so = (::sockaddr_in *)&addr;
                inet_ntop(AF_INET, &so->sin_addr, str, INET_ADDRSTRLEN);
                callback(SL::NET::sockaddr((unsigned char *)so, sizeof(sockaddr_in), str, so->sin_port, AddressFamily::IPV4));
            }
            else { // AF_INET6
                auto so = (::sockaddr_in6 *)&addr;
                inet_ntop(AF_INET6, &so->sin6_addr, str, INET_ADDRSTRLEN);
                callback(SL::NET::sockaddr((unsigned char *)so, sizeof(sockaddr_in6), str, so->sin6_port, AddressFamily::IPV6));
            }
            return StatusCode::SC_SUCCESS;
        }
        return TranslateError();
    }

    StatusCode PlatformSocket::getsockopt(DEBUGTag, const std::function<void(const SockOptStatus &)> &callback) const
    {
        int value = 0;
        SOCKLEN_T valuelen = sizeof(value);
        if (::getsockopt(Handle_.value, SOL_SOCKET, SO_DEBUG, (char *)&value, &valuelen) == 0) {
            callback(value != 0 ? SockOptStatus::ENABLED : SockOptStatus::DISABLED);
            return StatusCode::SC_SUCCESS;
        }
        return TranslateError();
    }

    StatusCode PlatformSocket::getsockopt(ACCEPTCONNTag, const std::function<void(const SockOptStatus &)> &callback) const
    {
        int value = 0;
        SOCKLEN_T valuelen = sizeof(value);
        if (::getsockopt(Handle_.value, SOL_SOCKET, SO_ACCEPTCONN, (char *)&value, &valuelen) == 0) {
            callback(value != 0 ? SockOptStatus::ENABLED : SockOptStatus::DISABLED);
            return StatusCode::SC_SUCCESS;
        }
        return TranslateError();
    }

    StatusCode PlatformSocket::getsockopt(BROADCASTTag, const std::function<void(const SockOptStatus &)> &callback) const
    {
        int value = 0;
        SOCKLEN_T valuelen = sizeof(value);
        if (::getsockopt(Handle_.value, SOL_SOCKET, SO_BROADCAST, (char *)&value, &valuelen) == 0) {
            callback(value != 0 ? SockOptStatus::ENABLED : SockOptStatus::DISABLED);
            return StatusCode::SC_SUCCESS;
        }
        return TranslateError();
    }

    StatusCode PlatformSocket::getsockopt(REUSEADDRTag, const std::function<void(const SockOptStatus &)> &callback) const
    {
        int value = 0;
        SOCKLEN_T valuelen = sizeof(value);
        if (::getsockopt(Handle_.value, SOL_SOCKET, SO_REUSEADDR, (char *)&value, &valuelen) == 0) {
            callback(value != 0 ? SockOptStatus::ENABLED : SockOptStatus::DISABLED);
            return StatusCode::SC_SUCCESS;
        }
        return TranslateError();
    }

    StatusCode PlatformSocket::getsockopt(KEEPALIVETag, const std::function<void(const SockOptStatus &)> &callback) const
    {
        int value = 0;
        SOCKLEN_T valuelen = sizeof(value);
        if (::getsockopt(Handle_.value, SOL_SOCKET, SO_KEEPALIVE, (char *)&value, &valuelen) == 0) {
            callback(value != 0 ? SockOptStatus::ENABLED : SockOptStatus::DISABLED);
            return StatusCode::SC_SUCCESS;
        }
        return TranslateError();
    }

    StatusCode PlatformSocket::getsockopt(LINGERTag, const std::function<void(const LingerOption &)> &callback) const
    {
        linger value;
        SOCKLEN_T valuelen = sizeof(value);
        if (::getsockopt(Handle_.value, SOL_SOCKET, SO_LINGER, (char *)&value, &valuelen) == 0) {
            callback(LingerOption{value.l_onoff == 0 ? LingerOptions::LINGER_OFF : LingerOptions::LINGER_ON, std::chrono::seconds(value.l_linger)});
            return StatusCode::SC_SUCCESS;
        }
        return TranslateError();
    }

    StatusCode PlatformSocket::getsockopt(OOBINLINETag, const std::function<void(const SockOptStatus &)> &callback) const
    {
        int value = 0;
        SOCKLEN_T valuelen = sizeof(value);
        if (::getsockopt(Handle_.value, SOL_SOCKET, SO_OOBINLINE, (char *)&value, &valuelen) == 0) {
            callback(value != 0 ? SockOptStatus::ENABLED : SockOptStatus::DISABLED);
            return StatusCode::SC_SUCCESS;
        }
        return TranslateError();
    }

    StatusCode PlatformSocket::getsockopt(SNDBUFTag, const std::function<void(const int &)> &callback) const
    {
        int value = 0;
        SOCKLEN_T valuelen = sizeof(value);
        if (::getsockopt(Handle_.value, SOL_SOCKET, SO_SNDBUF, (char *)&value, &valuelen) == 0) {
            callback(value);
            return StatusCode::SC_SUCCESS;
        }
        return TranslateError();
    }

    StatusCode PlatformSocket::getsockopt(RCVBUFTag, const std::function<void(const int &)> &callback) const
    {
        int value = 0;
        SOCKLEN_T valuelen = sizeof(value);
        if (::getsockopt(Handle_.value, SOL_SOCKET, SO_RCVBUF, (char *)&value, &valuelen) == 0) {
            callback(value);
            return StatusCode::SC_SUCCESS;
        }
        return TranslateError();
    }

    StatusCode PlatformSocket::getsockopt(SNDTIMEOTag, const std::function<void(const std::chrono::seconds &)> &callback) const
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
                callback(std::chrono::seconds(value.tv_sec));// convert from ms to seconds
                return StatusCode::SC_SUCCESS;
        }
#endif
        return TranslateError();
    }

    StatusCode PlatformSocket::getsockopt(RCVTIMEOTag, const std::function<void(const std::chrono::seconds &)> &callback) const
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
                callback(std::chrono::seconds(value.tv_sec));// convert from ms to seconds
                return StatusCode::SC_SUCCESS;
        }
#endif
        return TranslateError();
    }

    StatusCode PlatformSocket::getsockopt(ERRORTag, const std::function<void(const int &)> &callback) const
    {
        int value = 0;
        SOCKLEN_T valuelen = sizeof(value);
        if (::getsockopt(Handle_.value, SOL_SOCKET, SO_ERROR, (char *)&value, &valuelen) == 0) {
            callback(value);
            return StatusCode::SC_SUCCESS;
        }
        return TranslateError();
    }

    StatusCode PlatformSocket::getsockopt(NODELAYTag, const std::function<void(const SockOptStatus &)> &callback) const
    {
        int value = 0;
        SOCKLEN_T valuelen = sizeof(value);
        if (::getsockopt(Handle_.value, IPPROTO_TCP, TCP_NODELAY, (char *)&value, &valuelen) == 0) {
            callback(value != 0 ? SockOptStatus::ENABLED : SockOptStatus::DISABLED);
            return StatusCode::SC_SUCCESS;
        }
        return TranslateError();
    }

    StatusCode PlatformSocket::getsockopt(BLOCKINGTag, const std::function<void(const Blocking_Options &)> &callback) const
    {
#ifdef _WIN32
        return StatusCode::SC_NOTSUPPORTED;
#else
        long arg = 0;
        if ((arg = fcntl(Handle_.value, F_GETFL, NULL)) < 0) {
            return TranslateError();
        }
        callback((arg & O_NONBLOCK) != 0 ? Blocking_Options::NON_BLOCKING : Blocking_Options::BLOCKING);
        return StatusCode::SC_SUCCESS;
#endif
    }
    StatusCode PlatformSocket::setsockopt(DEBUGTag, SockOptStatus b)
    {
        int value = b == SockOptStatus::ENABLED ? 1 : 0;
        int valuelen = sizeof(value);
        if (::setsockopt(Handle_.value, SOL_SOCKET, SO_DEBUG, (char *)&value, valuelen) == 0) {
            return StatusCode::SC_SUCCESS;
        }
        return TranslateError();
    }

    StatusCode PlatformSocket::setsockopt(BROADCASTTag, SockOptStatus b)
    {
        int value = b == SockOptStatus::ENABLED ? 1 : 0;
        int valuelen = sizeof(value);
        if (::setsockopt(Handle_.value, SOL_SOCKET, SO_BROADCAST, (char *)&value, valuelen) == 0) {
            return StatusCode::SC_SUCCESS;
        }
        return TranslateError();
    }

    StatusCode PlatformSocket::setsockopt(REUSEADDRTag, SockOptStatus b)
    {
        int value = b == SockOptStatus::ENABLED ? 1 : 0;
        int valuelen = sizeof(value);
        if (::setsockopt(Handle_.value, SOL_SOCKET, SO_BROADCAST, (char *)&value, valuelen) == 0) {
            return StatusCode::SC_SUCCESS;
        }
        return TranslateError();
    }

    StatusCode PlatformSocket::setsockopt(KEEPALIVETag, SockOptStatus b)
    {
        int value = b == SockOptStatus::ENABLED ? 1 : 0;
        int valuelen = sizeof(value);
        if (::setsockopt(Handle_.value, SOL_SOCKET, SO_KEEPALIVE, (char *)&value, valuelen) == 0) {
            return StatusCode::SC_SUCCESS;
        }
        return TranslateError();
    }

    StatusCode PlatformSocket::setsockopt(LINGERTag, LingerOption o)
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

    StatusCode PlatformSocket::setsockopt(OOBINLINETag, SockOptStatus b)
    {
        int value = b == SockOptStatus::ENABLED ? 1 : 0;
        int valuelen = sizeof(value);
        if (::setsockopt(Handle_.value, SOL_SOCKET, SO_OOBINLINE, (char *)&value, valuelen) == 0) {
            return StatusCode::SC_SUCCESS;
        }
        return TranslateError();
    }

    StatusCode PlatformSocket::setsockopt(SNDBUFTag, int b)
    {
        int value = b;
        int valuelen = sizeof(value);
        if (::setsockopt(Handle_.value, SOL_SOCKET, SO_SNDBUF, (char *)&value, valuelen) == 0) {
            return StatusCode::SC_SUCCESS;
        }
        return TranslateError();
    }

    StatusCode PlatformSocket::setsockopt(RCVBUFTag, int b)
    {
        int value = b;
        int valuelen = sizeof(value);
        if (::setsockopt(Handle_.value, SOL_SOCKET, SO_RCVBUF, (char *)&value, valuelen) == 0) {
            return StatusCode::SC_SUCCESS;
        }
        return TranslateError();
    }

    StatusCode PlatformSocket::setsockopt(SNDTIMEOTag, std::chrono::seconds sec)
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

    StatusCode PlatformSocket::setsockopt(RCVTIMEOTag, std::chrono::seconds sec)
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

    StatusCode PlatformSocket::setsockopt(NODELAYTag, SockOptStatus b)
    {
        int value = b == SockOptStatus::ENABLED ? 1 : 0;
        int valuelen = sizeof(value);
        if (::setsockopt(Handle_.value, IPPROTO_TCP, TCP_NODELAY, (char *)&value, valuelen) == 0) {
            return StatusCode::SC_SUCCESS;
        }
        return TranslateError();
    }

    StatusCode PlatformSocket::setsockopt(BLOCKINGTag, Blocking_Options b)
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

} // namespace NET
} // namespace SL
