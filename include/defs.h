#pragma once

#if __APPLE__
#include <experimental/optional>
#else
#include <optional>
#endif
#include <algorithm>
#include <assert.h>
#include <atomic>
#include <chrono>
#include <memory>
#include <stdint.h>
#include <string>
#include <thread>
#include <vector>

#if _WIN32
#include <WinSock2.h>
#include <Windows.h>
#include <Ws2tcpip.h>
#include <mswsock.h>
#ifndef SOCKLEN_T
typedef int SOCKLEN_T;
#endif
#else
#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#ifndef SOCKLEN_T
typedef socklen_t SOCKLEN_T;
#endif
#endif

#ifndef INVALID_SOCKET
#define INVALID_SOCKET -1
#endif
#ifndef SOCKET_ERROR
#define SOCKET_ERROR -1
#endif

namespace SL {
namespace NET {

    struct SocketHandleTag {
    };
    struct PorNumbertTag {
    };
    struct ThreadCountTag {
    };
    template <typename T, typename Meaning> struct Explicit {
        Explicit(T value) : value(value) {}
        T value;
    };
    struct DEBUGTag {
    };
    struct ACCEPTCONNTag {
    };
    struct BROADCASTTag {
    };
    struct REUSEADDRTag {
    };
    struct KEEPALIVETag {
    };
    struct LINGERTag {
    };
    struct OOBINLINETag {
    };
    struct SNDBUFTag {
    };
    struct RCVBUFTag {
    };
    struct SNDTIMEOTag {
    };
    struct RCVTIMEOTag {
    };
    struct ERRORTag {
    };
    struct NODELAYTag {
    };
    struct BLOCKINGTag {
    };
    typedef Explicit<unsigned int, ThreadCountTag> ThreadCount;
    typedef Explicit<unsigned short, PorNumbertTag> PortNumber;
    enum class Blocking_Options { BLOCKING, NON_BLOCKING };
    enum class[[nodiscard]] StatusCode{SC_EAGAIN,   SC_EWOULDBLOCK, SC_EBADF,     SC_ECONNRESET, SC_EINTR,        SC_EINVAL,    SC_ENOTCONN,
                                       SC_ENOTSOCK, SC_EOPNOTSUPP,  SC_ETIMEDOUT, SC_CLOSED,     SC_NOTSUPPORTED, SC_PENDINGIO, SC_SUCCESS = 0};
    enum class LingerOptions { LINGER_OFF, LINGER_ON };
    enum class SockOptStatus { ENABLED, DISABLED };
    enum class AddressFamily { IPV4, IPV6, IPANY };
    enum class SocketStatus { CLOSED, CONNECTING, OPEN };
    enum class ShutDownOptions { SHUTDOWN_READ, SHUTDOWN_WRITE, SHUTDOWN_BOTH };
    struct LingerOption {
        LingerOptions l_onoff;         /* option on/off */
        std::chrono::seconds l_linger; /* linger time */
    };
#ifdef _WIN32
#if ((UINTPTR_MAX) == (UINT_MAX))
    typedef Explicit<unsigned short, SocketHandleTag> SocketHandle;
#else
    typedef Explicit<unsigned long long, SocketHandleTag> SocketHandle;
#endif
#else
    typedef Explicit<int, SocketHandleTag> SocketHandle;
#endif 
    enum IO_OPERATION : unsigned int { IoRead, IoWrite, IoConnect, IoAccept };

    class SocketAddress {
        unsigned char SocketImpl[65] = {0};
        int SocketImplLen = 0;
        std::string Host;
        unsigned short Port = 0;
        AddressFamily Family = AddressFamily::IPV4;

      public:
        SocketAddress() {}

        SocketAddress(const SocketAddress &addr) : SocketImplLen(addr.SocketImplLen), Host(addr.Host), Port(addr.Port), Family(addr.Family)
        {
            memcpy(SocketImpl, addr.SocketImpl, sizeof(SocketImpl));
        }
        SocketAddress(unsigned char *buffer, int len, const char *host, unsigned short port, AddressFamily family)
        {
            assert(static_cast<size_t>(len) < sizeof(SocketImpl));
            memcpy(SocketImpl, buffer, len);
            SocketImplLen = len;
            Host = host;
            Port = port;
            Family = family;
        }

        const unsigned char *getSocketAddr() const { return SocketImpl; }
        int getSocketAddrLen() const { return SocketImplLen; }
        const std::string &SocketAddress::getHost() const { return Host; }
        unsigned short getPort() const { return Port; }
        AddressFamily getFamily() const { return Family; }
    };
    inline const unsigned char *SocketAddr(const SocketAddress &s) { return s.getSocketAddr(); }
    inline int SocketAddrLen(const SocketAddress &s) { return s.getSocketAddrLen(); }
    inline const std::string &Host(const SocketAddress &s) { return s.getHost(); }
    inline unsigned short Port(const SocketAddress &s) { return s.getPort(); }
    inline AddressFamily Family(const SocketAddress &s) { return s.getFamily(); }

    template <class CALLBACKLIFETIMEOBJECT> class RW_Context {
      public:
        typedef void (*Handler)(StatusCode, CALLBACKLIFETIMEOBJECT&);
#ifdef _WIN32
        WSAOVERLAPPED Overlapped = {0};
#endif
      private:
        std::atomic<Handler> completionhandler;
        int remaining_bytes;
        CALLBACKLIFETIMEOBJECT UserData;

      public:
        unsigned char *buffer = nullptr;
        RW_Context() { clear(); }
        RW_Context(const RW_Context &) { clear(); }
        void setUserData(CALLBACKLIFETIMEOBJECT &userdata) { UserData = std::move(userdata); }
        CALLBACKLIFETIMEOBJECT &getUserData() { return UserData; }
        void setCompletionHandler(Handler c)
        {
#if _WIN32
            Overlapped = {0};
#endif
            completionhandler.store(c, std::memory_order_relaxed);
        }
        Handler getCompletionHandler()
        {
            auto h = completionhandler.load(std::memory_order_relaxed);
            if (h) {
                while (!completionhandler.compare_exchange_weak(h, nullptr, std::memory_order_release, std::memory_order_relaxed))
                    ; // empty on purpose
            }
            return h;
        }
        void setRemainingBytes(int remainingbytes)
        {
            auto p = (remainingbytes & ~0xC0000000);
            auto p1 = (remaining_bytes & 0xC0000000);

            remaining_bytes = p | p1;
        }
        int getRemainingBytes() const { return remaining_bytes & ~0xC0000000; }
        void setEvent(IO_OPERATION op) { remaining_bytes = getRemainingBytes() | (op << 30); }
        IO_OPERATION getEvent() const { return static_cast<IO_OPERATION>((remaining_bytes >> 30) & 0x00000003); }

        void clear()
        {
            remaining_bytes = 0;
            buffer = nullptr;
            auto p = std::move(UserData); //clear userdata
            completionhandler = nullptr;
        }
    };

    static StatusCode TranslateError(int *errcode = nullptr)
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
    }

} // namespace NET
} // namespace SL
