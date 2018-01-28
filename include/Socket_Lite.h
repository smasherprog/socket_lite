#pragma once

#ifdef _WIN32
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

#include <chrono>
#include <functional>
#include <memory>
#include <optional>
#include <stdint.h>
#include <variant>
#include <vector>

#if defined(WINDOWS) || defined(WIN32)
#if defined(WS_LITE_DLL)
#define SOCKET_LITE_EXTERN __declspec(dllexport)
#else
#define SOCKET_LITE_EXTERN
#endif
#else
#define SOCKET_LITE_EXTERN
#endif
namespace SL {
namespace NET {
    template <typename T, typename Meaning> struct Explicit {
        Explicit() {}
        Explicit(T value) : value(value) {}
        inline operator T() const { return value; }
        T value;
    };
    namespace INTERNAL {
        struct PorNumbertTag {
        };
        struct ThreadCountTag {
        };
    } // namespace INTERNAL

    enum class Socket_Options {
        O_DEBUG,
        O_ACCEPTCONN,
        O_BROADCAST,
        O_REUSEADDR,
        O_KEEPALIVE,
        O_LINGER,
        O_OOBINLINE,
        O_EXCLUSIVEADDRUSE,
        O_SNDBUF,
        O_RCVBUF,
        O_SNDTIMEO,
        O_RCVTIMEO,
        O_ERROR,
        O_NODELAY,
        O_BLOCKING
    };
    enum class Blocking_Options { BLOCKING, NON_BLOCKING };
#if ((UINTPTR_MAX) == (UINT_MAX))
    typedef int Bytes_Transfered;
#else
    typedef long long int Bytes_Transfered;
#endif
#ifdef _WIN32
#if ((UINTPTR_MAX) == (UINT_MAX))
    typedef unsigned int Platform_Socket;
#else

    typedef unsigned long long Platform_Socket;
#endif
#else
    typedef int Platform_Socket;
#endif

    typedef Explicit<unsigned short, INTERNAL::ThreadCountTag> ThreadCount;
    typedef Explicit<unsigned short, INTERNAL::PorNumbertTag> PortNumber;

    enum class ConnectionAttemptStatus { SuccessfullConnect, FailedConnect };
    enum SocketErrors {
        SE_EAGAIN = INT_MIN,
        SE_EWOULDBLOCK,
        SE_EBADF,
        SE_ECONNRESET,
        SE_EINTR,
        SE_EINVAL,
        SE_ENOTCONN,
        SE_ENOTSOCK,
        SE_EOPNOTSUPP,
        SE_ETIMEDOUT
    };

    enum class Linger_Options { LINGER_OFF, LINGER_ON };
    enum class SockOptStatus { ENABLED, DISABLED };
    struct Linger_Option {
        Linger_Options l_onoff;        /* option on/off */
        std::chrono::seconds l_linger; /* linger time */
    };
    enum class Address_Family { IPV4, IPV6 };
    class SOCKET_LITE_EXTERN sockaddr {
        sockaddr_storage sockaddr_;
        int sockaddr_len = 0;
        std::string Host;
        unsigned short Port = 0;
        Address_Family Family = Address_Family::IPV4;

      public:
        sockaddr() {}
        sockaddr(unsigned char *buffer, int len, char *host, unsigned short port, Address_Family family);
        sockaddr(const sockaddr &addr) : SocketImplLen(addr.SocketImplLen), Host(addr.Host), Family(addr.Family), Port(addr.Port)
        {
            memcpy(SocketImpl, addr.SocketImpl, sizeof(SocketImpl));
        }
        const unsigned char *get_SocketAddr() const;
        int get_SocketAddrLen() const;
        std::string get_Host() const;
        unsigned short get_Port() const;
        Address_Family get_Family() const;
    };

    template <class T> T getsockopt(Socket_Options option, Platform_Socket handle)
    {
        int value = 0;
        int valuelen = sizeof(value);
        int ret = -1;
        switch (option) {
        case SL::NET::Socket_Options::O_DEBUG:
            ret = ::getsockopt(handle, SOL_SOCKET, SO_DEBUG, (char *)&value, &valuelen);
            break;
        case SL::NET::Socket_Options::O_ACCEPTCONN:
            ret = ::getsockopt(handle, SOL_SOCKET, SO_ACCEPTCONN, (char *)&value, &valuelen);
            break;
        case SL::NET::Socket_Options::O_BROADCAST:
            ret = ::getsockopt(handle, SOL_SOCKET, SO_BROADCAST, (char *)&value, &valuelen);
            break;
        case SL::NET::Socket_Options::O_REUSEADDR:
            ret = ::getsockopt(handle, SOL_SOCKET, SO_REUSEADDR, (char *)&value, &valuelen);
            break;
        case SL::NET::Socket_Options::O_KEEPALIVE:
            ret = ::getsockopt(handle, SOL_SOCKET, SO_KEEPALIVE, (char *)&value, &valuelen);
            break;
        case SL::NET::Socket_Options::O_LINGER: {

            linger lvalue;
            int lvaluelen = sizeof(lvalue);
            if (::getsockopt(handle, SOL_SOCKET, SO_LINGER, (char *)&lvalue, &lvaluelen) == 0) {
                return std::optional<Linger_Option>(
                    {lvalue.l_onoff == 0 ? Linger_Options::LINGER_OFF : Linger_Options::LINGER_ON, std::chrono::seconds(lvalue.l_linger)});
            }
        } break;
        case SL::NET::Socket_Options::O_OOBINLINE:
            ret = ::getsockopt(handle, SOL_SOCKET, SO_OOBINLINE, (char *)&value, &valuelen);
            break;
        case SL::NET::Socket_Options::O_EXCLUSIVEADDRUSE:
            ret = ::getsockopt(handle, SOL_SOCKET, SO_EXCLUSIVEADDRUSE, (char *)&value, &valuelen);
            break;
        case SL::NET::Socket_Options::O_SNDBUF:
            ret = ::getsockopt(handle, SOL_SOCKET, SO_SNDBUF, (char *)&value, &valuelen);
            break;
        case SL::NET::Socket_Options::O_RCVBUF:
            ret = ::getsockopt(handle, SOL_SOCKET, SO_RCVBUF, (char *)&value, &valuelen);
            break;
        case SL::NET::Socket_Options::O_SNDTIMEO: {
#ifdef _WIN32

            DWORD dvalue = 0;
            int dvaluelen = sizeof(dvalue);
            if (::getsockopt(handle, SOL_SOCKET, SO_SNDTIMEO, (char *)&dvalue, &dvaluelen) == 0) {
                return std::optional<std::chrono::seconds>(dvalue * 1000); // convert from ms to seconds
            }
#else
            timeval tvalue = {0};
            int tvaluelen = sizeof(tvalue);
            if (::getsockopt(handle, SOL_SOCKET, SO_SNDTIMEO, (char *)&tvalue, &tvaluelen) == 0) {
                return std::optional<std::chrono::seconds>(tvalue.tv_sec);
            }
#endif
        } break;
        case SL::NET::Socket_Options::O_RCVTIMEO:

#ifdef _WIN32
            DWORD dvalue = 0;
            int dvaluelen = sizeof(dvalue);
            if (::getsockopt(handle, SOL_SOCKET, SO_RCVTIMEO, (char *)&dvalue, &dvaluelen) == 0) {
                return std::optional<std::chrono::seconds>(dvalue * 1000); // convert from ms to seconds
            }
#else
            timeval tvalue = {0};
            int tvaluelen = sizeof(tvalue);
            if (::getsockopt(handle, SOL_SOCKET, SO_RCVTIMEO, (char *)&tvalue, &tvaluelen) == 0) {
                return std::optional<std::chrono::seconds>(tvalue.tv_sec);
            }
#endif

            break;
        case SL::NET::Socket_Options::O_ERROR:
            if (::getsockopt(handle, SOL_SOCKET, SO_ERROR, (char *)&value, &valuelen) == 0) {
                return std::optional<int>(value);
            }
            break;
        case SL::NET::Socket_Options::O_NODELAY:
            ret = ::getsockopt(handle, IPPROTO_TCP, TCP_NODELAY, (char *)&value, &valuelen);
            break;
        case SL::NET::Socket_Options::O_BLOCKING:

#ifndef _WIN32
            if (auto arg = fcntl(handle, F_GETFL, NULL); arg >= 0) {
                return std::optional<Blocking_Options>((arg & O_NONBLOCK) != 0 ? Blocking_Options::NON_BLOCKING : Blocking_Options::BLOCKING);
            }
#endif
            break;
        default:
            break;
        }
        if (ret == 0) {
            return std::optional<bool>(value != 0);
        }
        return std::nullopt;
    }
    std::vector<sockaddr> SOCKET_LITE_EXTERN getaddrinfo(char *nodename, PortNumber pServiceName, Address_Family family);
    class SOCKET_LITE_EXTERN IIO_Context {
      public:
        virtual ~IIO_Context() {}
        virtual void run(ThreadCount threadcount) = 0;
    };
    std::shared_ptr<IIO_Context> SOCKET_LITE_EXTERN CreateIO_Context();

    class SOCKET_LITE_EXTERN ISocket : std::enable_shared_from_this<ISocket> {

        bool setsockopt_O_DEBUG(bool b) const;
        bool setsockopt_O_BROADCAST(bool b) const;
        bool setsockopt_O_REUSEADDR(bool b) const;
        bool setsockopt_O_KEEPALIVE(bool b) const;
        bool setsockopt_O_LINGER(Linger_Option o) const;
        bool setsockopt_O_OOBINLINE(bool b) const;
        bool setsockopt_O_EXCLUSIVEADDRUSE(bool b) const;
        bool setsockopt_O_SNDBUF(int b) const;
        bool setsockopt_O_RCVBUF(int b) const;
        bool setsockopt_O_SNDTIMEO(std::chrono::seconds sec) const;
        bool setsockopt_O_RCVTIMEO(std::chrono::seconds sec) const;
        bool setsockopt_O_NODELAY(bool b) const;
        bool setsockopt_O_BLOCKING(Blocking_Options b) const;

        template <Socket_Options SO> struct setsockopt_factory_impl;
        template <> struct setsockopt_factory_impl<Socket_Options::O_DEBUG> {
            static auto setsockopt_(ISocket *s, bool b) { return s->setsockopt_O_DEBUG(b); }
        };
        template <> struct setsockopt_factory_impl<Socket_Options::O_BROADCAST> {
            static auto setsockopt_(ISocket *s, bool b) { return s->setsockopt_O_BROADCAST(b); }
        };
        template <> struct setsockopt_factory_impl<Socket_Options::O_REUSEADDR> {
            static auto setsockopt_(ISocket *s, bool b) { return s->setsockopt_O_REUSEADDR(b); }
        };
        template <> struct setsockopt_factory_impl<Socket_Options::O_KEEPALIVE> {
            static auto setsockopt_(ISocket *s, bool b) { return s->setsockopt_O_KEEPALIVE(b); }
        };
        template <> struct setsockopt_factory_impl<Socket_Options::O_LINGER> {
            static auto setsockopt_(ISocket *s, Linger_Option b) { return s->setsockopt_O_LINGER(b); }
        };
        template <> struct setsockopt_factory_impl<Socket_Options::O_OOBINLINE> {
            static auto setsockopt_(ISocket *s, bool b) { return s->setsockopt_O_OOBINLINE(b); }
        };
        template <> struct setsockopt_factory_impl<Socket_Options::O_EXCLUSIVEADDRUSE> {
            static auto setsockopt_(ISocket *s, bool b) { return s->setsockopt_O_EXCLUSIVEADDRUSE(b); }
        };
        template <> struct setsockopt_factory_impl<Socket_Options::O_SNDBUF> {
            static auto setsockopt_(ISocket *s, int b) { return s->setsockopt_O_SNDBUF(b); }
        };
        template <> struct setsockopt_factory_impl<Socket_Options::O_RCVBUF> {
            static auto setsockopt_(ISocket *s, int b) { return s->setsockopt_O_RCVBUF(b); }
        };
        template <> struct setsockopt_factory_impl<Socket_Options::O_RCVTIMEO> {
            static auto setsockopt_(ISocket *s, std::chrono::seconds b) { return s->setsockopt_O_RCVTIMEO(b); }
        };
        template <> struct setsockopt_factory_impl<Socket_Options::O_SNDTIMEO> {
            static auto setsockopt_(ISocket *s, std::chrono::seconds b) { return s->setsockopt_O_SNDTIMEO(b); }
        };
        template <> struct setsockopt_factory_impl<Socket_Options::O_NODELAY> {
            static auto setsockopt_(ISocket *s, bool b) { return s->setsockopt_O_NODELAY(b); }
        };
        template <> struct setsockopt_factory_impl<Socket_Options::O_BLOCKING> {
            static auto setsockopt_(ISocket *s, Blocking_Options b) { return s->setsockopt_O_BLOCKING(b); }
        };

      protected:
        Platform_Socket handle = INVALID_SOCKET;

      public:
        ISocket() {}
        virtual ~ISocket() { close(); };
        template <Socket_Options SO, class T> T getsockopt() const { return SL::NET::getsockopt(SO, handle); }
        template <Socket_Options SO, typename... Args> auto setsockopt(Args &&... args)
        {
            return setsockopt_factory_impl<SO>::setsockopt_(this, std::forward<Args>(args)...);
        }
        std::optional<SL::NET::sockaddr> getpeername() const;
        std::optional<SL::NET::sockaddr> getsockname() const;
        bool bind(sockaddr addr);
        bool listen(int backlog) const;
        ConnectionAttemptStatus connect(SL::NET::sockaddr &addresses) const;
        virtual void connect(std::shared_ptr<IIO_Context> &iocontext, SL::NET::sockaddr &address,
                             const std::function<void(ConnectionAttemptStatus)> &&) = 0;

        virtual void recv(size_t buffer_size, unsigned char *buffer, const std::function<void(Bytes_Transfered)> &&handler) = 0;
        virtual void send(size_t buffer_size, unsigned char *buffer, const std::function<void(Bytes_Transfered)> &&handler) = 0;
        virtual void close()
        {
            if (handle != INVALID_SOCKET) {
                closesocket(handle);
            }
            handle = INVALID_SOCKET;
        }
        Platform_Socket get_handle() const { return handle; }
        void set_handle(Platform_Socket h);
    };
    std::shared_ptr<ISocket> SOCKET_LITE_EXTERN CreateSocket(std::shared_ptr<IIO_Context> &iocontext);

    class SOCKET_LITE_EXTERN IListener {
      public:
        virtual ~IListener() {}
        virtual void close() = 0;
        virtual void async_accept(const std::function<void(const std::shared_ptr<ISocket> &)> &&handler) = 0;
    };
    std::shared_ptr<IListener> SOCKET_LITE_EXTERN CreateListener(const std::shared_ptr<IIO_Context> &iocontext,
                                                                 std::shared_ptr<ISocket> &&listensocket); // listener steals the socket

} // namespace NET
} // namespace SL
