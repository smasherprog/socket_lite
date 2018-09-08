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

namespace SL::NET {

struct SocketHandleTag {
};
struct AsyncSocketHandleTag {
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
enum class [[nodiscard]] StatusCode {
    SC_EAGAIN, SC_EWOULDBLOCK, SC_EBADF, SC_ECONNRESET, SC_EINTR, SC_EINVAL, SC_ENOTCONN, SC_ENOTSOCK, SC_EOPNOTSUPP, SC_ETIMEDOUT, SC_CLOSED,
        SC_NOTSUPPORTED, SC_PENDINGIO, SC_SUCCESS = 0
};
enum class LingerOptions { LINGER_OFF, LINGER_ON };
enum class SockOptStatus { ENABLED, DISABLED };
enum AddressFamily : unsigned short { IPV4 = AF_INET, IPV6 = AF_INET6, IPANY = AF_UNSPEC };
enum class SocketStatus { CLOSED, CONNECTING, OPEN };
enum class ShutDownOptions { SHUTDOWN_READ, SHUTDOWN_WRITE, SHUTDOWN_BOTH };
struct LingerOption {
    LingerOptions l_onoff;         /* option on/off */
    std::chrono::seconds l_linger; /* linger time */
};
#ifdef _WIN32
#if ((UINTPTR_MAX) == (UINT_MAX))
typedef Explicit<unsigned short, SocketHandleTag> SocketHandle;
typedef Explicit<unsigned short, AsyncSocketHandleTag> AsyncSocketHandle;
#else
typedef Explicit<unsigned long long, SocketHandleTag> SocketHandle;
typedef Explicit<unsigned long long, SocketHandleTag> AsyncSocketHandle;
#endif
#else

struct IOCompleteHeader {
    int IO_op : 2;
    int SocketId : 30;
};
typedef Explicit<int, SocketHandleTag> SocketHandle;
typedef Explicit<int, SocketHandleTag> AsyncSocketHandle;
#endif
enum IO_OPERATION : unsigned int { IoRead, IoWrite, IoConnect, IoAccept };

class SocketAddress {
    ::sockaddr_storage Storage;
    int Length;

  public:
    SocketAddress() noexcept : Length(0), Storage({}) {}
    SocketAddress(SocketAddress &&addr) noexcept : Length(addr.Length)
    {
        memcpy(&Storage, &addr.Storage, addr.Length);
        addr.Length = 0;
    }
    SocketAddress(const SocketAddress &addr) noexcept : Length(addr.Length) { memcpy(&Storage, &addr.Storage, addr.Length); }
    SocketAddress(::sockaddr *buffer, socklen_t len) : Length(static_cast<int>(len))
    {
        assert(len < sizeof(Storage));
        memcpy(&Storage, buffer, len);

    }
    SocketAddress(::sockaddr_storage *buffer, socklen_t len) noexcept
    {
        assert(len < sizeof(Storage));
        memcpy(&Storage, buffer, len);
        Length = static_cast<int>(len);
    }

    const sockaddr *getSocketAddr() const noexcept { return reinterpret_cast<const ::sockaddr *>(&Storage); }
    int getSocketAddrLen() const noexcept { return Length; }
    std::string getHost() const noexcept
    {
        char str[INET_ADDRSTRLEN] = {};
        auto sockin = reinterpret_cast<const ::sockaddr_in *>(&Storage);
        inet_ntop(Storage.ss_family, &(sockin->sin_addr), str, INET_ADDRSTRLEN);
        return std::string(str);
    }
    unsigned short getPort() const noexcept
    {
        // both ipv6 and ipv4 structs have their port in the same place!
        auto sockin = reinterpret_cast<const sockaddr_in *>(&Storage);
        return ntohs(sockin->sin_port);
    }
    AddressFamily getFamily() const noexcept { return static_cast<AddressFamily>(Storage.ss_family); }
};

typedef std::function<void(StatusCode)> SocketHandler;

class RW_Context {
  public:
#if _WIN32
    WSAOVERLAPPED Overlapped = {};
#endif
    private : SocketHandler completionhandler;
    std::atomic<int> completioncounter;
    int remaining_bytes = 0;

  public:
    unsigned char *buffer = nullptr;
    RW_Context() noexcept { clear(); }
    RW_Context(const RW_Context &) noexcept { clear(); }
    template <class T> void setCompletionHandler(const T &c)
    {
#if _WIN32
        Overlapped.Internal = Overlapped.InternalHigh = 0;
        Overlapped.Offset = Overlapped.OffsetHigh = 0;
        Overlapped.Pointer = Overlapped.hEvent = 0;
#endif
        completioncounter.store(1, std::memory_order_relaxed);
        completionhandler = std::move(c);
    }
    bool hasCompletionHandler() const { return completionhandler.operator bool(); }
    SocketHandler getCompletionHandler()
    {
        if (completioncounter.fetch_sub(1, std::memory_order_relaxed) == 1) {
            return std::move(completionhandler);
        }
        SocketHandler h;
        return h;
    }
    constexpr void setRemainingBytes(int remainingbytes)
    {
        auto p = (remainingbytes & ~0xC0000000);
        auto p1 = (remaining_bytes & 0xC0000000);

        remaining_bytes = p | p1;
    }
    constexpr int getRemainingBytes() const { return remaining_bytes & ~0xC0000000; }
    constexpr void setEvent(IO_OPERATION op) { remaining_bytes = getRemainingBytes() | (op << 30); }
    constexpr IO_OPERATION getEvent() const { return static_cast<IO_OPERATION>((remaining_bytes >> 30) & 0x00000003); }
    void clear()
    {
#if _WIN32
        Overlapped.Internal = Overlapped.InternalHigh = 0;
        Overlapped.Offset = Overlapped.OffsetHigh = 0;
        Overlapped.Pointer = Overlapped.hEvent = 0;
#endif
        completioncounter = 0;
        remaining_bytes = 0;
        buffer = nullptr;
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

} // namespace SL::NET
