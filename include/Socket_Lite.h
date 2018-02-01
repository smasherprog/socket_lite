#pragma once

#include <chrono>
#include <functional>
#include <memory>
#include <optional>
#include <stdint.h>
#include <variant>
#include <vector>

#if defined(WINDOWS) || defined(_WIN32)
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
        unsigned char SocketImpl[65] = {0};
        int SocketImplLen = 0;
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
    namespace INTERNAL {
        std::optional<SockOptStatus> SOCKET_LITE_EXTERN getsockopt_O_DEBUG(Platform_Socket handle);
        std::optional<SockOptStatus> SOCKET_LITE_EXTERN getsockopt_O_ACCEPTCONN(Platform_Socket handle);
        std::optional<SockOptStatus> SOCKET_LITE_EXTERN getsockopt_O_BROADCAST(Platform_Socket handle);
        std::optional<SockOptStatus> SOCKET_LITE_EXTERN getsockopt_O_REUSEADDR(Platform_Socket handle);
        std::optional<SockOptStatus> SOCKET_LITE_EXTERN getsockopt_O_KEEPALIVE(Platform_Socket handle);
        std::optional<Linger_Option> SOCKET_LITE_EXTERN getsockopt_O_LINGER(Platform_Socket handle);
        std::optional<SockOptStatus> SOCKET_LITE_EXTERN getsockopt_O_OOBINLINE(Platform_Socket handle);
        std::optional<SockOptStatus> SOCKET_LITE_EXTERN getsockopt_O_EXCLUSIVEADDRUSE(Platform_Socket handle);
        std::optional<int> SOCKET_LITE_EXTERN getsockopt_O_SNDBUF(Platform_Socket handle);
        std::optional<int> SOCKET_LITE_EXTERN getsockopt_O_RCVBUF(Platform_Socket handle);
        std::optional<std::chrono::seconds> SOCKET_LITE_EXTERN getsockopt_O_SNDTIMEO(Platform_Socket handle);
        std::optional<std::chrono::seconds> SOCKET_LITE_EXTERN getsockopt_O_RCVTIMEO(Platform_Socket handle);
        std::optional<int> SOCKET_LITE_EXTERN getsockopt_O_ERROR(Platform_Socket handle);
        std::optional<SockOptStatus> SOCKET_LITE_EXTERN getsockopt_O_NODELAY(Platform_Socket handle);
        std::optional<Blocking_Options> SOCKET_LITE_EXTERN getsockopt_O_BLOCKING(Platform_Socket handle);

        template <Socket_Options SO> struct getsockopt_factory_impl;
        template <> struct getsockopt_factory_impl<Socket_Options::O_DEBUG> {
            static auto getsockopt_(Platform_Socket handle) { return getsockopt_O_DEBUG(handle); }
        };
        template <> struct getsockopt_factory_impl<Socket_Options::O_ACCEPTCONN> {
            static auto getsockopt_(Platform_Socket handle) { return getsockopt_O_ACCEPTCONN(handle); }
        };
        template <> struct getsockopt_factory_impl<Socket_Options::O_BROADCAST> {
            static auto getsockopt_(Platform_Socket handle) { return getsockopt_O_BROADCAST(handle); }
        };
        template <> struct getsockopt_factory_impl<Socket_Options::O_REUSEADDR> {
            static auto getsockopt_(Platform_Socket handle) { return getsockopt_O_REUSEADDR(handle); }
        };
        template <> struct getsockopt_factory_impl<Socket_Options::O_KEEPALIVE> {
            static auto getsockopt_(Platform_Socket handle) { return getsockopt_O_KEEPALIVE(handle); }
        };
        template <> struct getsockopt_factory_impl<Socket_Options::O_LINGER> {
            static auto getsockopt_(Platform_Socket handle) { return getsockopt_O_LINGER(handle); }
        };
        template <> struct getsockopt_factory_impl<Socket_Options::O_OOBINLINE> {
            static auto getsockopt_(Platform_Socket handle) { return getsockopt_O_OOBINLINE(handle); }
        };
        template <> struct getsockopt_factory_impl<Socket_Options::O_EXCLUSIVEADDRUSE> {
            static auto getsockopt_(Platform_Socket handle) { return getsockopt_O_EXCLUSIVEADDRUSE(handle); }
        };
        template <> struct getsockopt_factory_impl<Socket_Options::O_SNDBUF> {
            static auto getsockopt_(Platform_Socket handle) { return getsockopt_O_SNDBUF(handle); }
        };
        template <> struct getsockopt_factory_impl<Socket_Options::O_RCVBUF> {
            static auto getsockopt_(Platform_Socket handle) { return getsockopt_O_RCVBUF(handle); }
        };
        template <> struct getsockopt_factory_impl<Socket_Options::O_RCVTIMEO> {
            static auto getsockopt_(Platform_Socket handle) { return getsockopt_O_RCVTIMEO(handle); }
        };
        template <> struct getsockopt_factory_impl<Socket_Options::O_SNDTIMEO> {
            static auto getsockopt_(Platform_Socket handle) { return getsockopt_O_SNDTIMEO(handle); }
        };
        template <> struct getsockopt_factory_impl<Socket_Options::O_ERROR> {
            static auto getsockopt_(Platform_Socket handle) { return getsockopt_O_ERROR(handle); }
        };
        template <> struct getsockopt_factory_impl<Socket_Options::O_NODELAY> {
            static auto getsockopt_(Platform_Socket handle) { return getsockopt_O_NODELAY(handle); }
        };
        template <> struct getsockopt_factory_impl<Socket_Options::O_BLOCKING> {
            static auto getsockopt_(Platform_Socket handle) { return getsockopt_O_BLOCKING(handle); }
        };

        bool SOCKET_LITE_EXTERN setsockopt_O_DEBUG(Platform_Socket handle, SockOptStatus b);
        bool SOCKET_LITE_EXTERN setsockopt_O_BROADCAST(Platform_Socket handle, SockOptStatus b);
        bool SOCKET_LITE_EXTERN setsockopt_O_REUSEADDR(Platform_Socket handle, SockOptStatus b);
        bool SOCKET_LITE_EXTERN setsockopt_O_KEEPALIVE(Platform_Socket handle, SockOptStatus b);
        bool SOCKET_LITE_EXTERN setsockopt_O_LINGER(Platform_Socket handle, Linger_Option o);
        bool SOCKET_LITE_EXTERN setsockopt_O_OOBINLINE(Platform_Socket handle, SockOptStatus b);
        bool SOCKET_LITE_EXTERN setsockopt_O_EXCLUSIVEADDRUSE(Platform_Socket handle, SockOptStatus b);
        bool SOCKET_LITE_EXTERN setsockopt_O_SNDBUF(Platform_Socket handle, int b);
        bool SOCKET_LITE_EXTERN setsockopt_O_RCVBUF(Platform_Socket handle, int b);
        bool SOCKET_LITE_EXTERN setsockopt_O_SNDTIMEO(Platform_Socket handle, std::chrono::seconds sec);
        bool SOCKET_LITE_EXTERN setsockopt_O_RCVTIMEO(Platform_Socket handle, std::chrono::seconds sec);
        bool SOCKET_LITE_EXTERN setsockopt_O_NODELAY(Platform_Socket handle, SockOptStatus b);
        bool SOCKET_LITE_EXTERN setsockopt_O_BLOCKING(Platform_Socket handle, Blocking_Options b);

        template <Socket_Options SO> struct setsockopt_factory_impl;
        template <> struct setsockopt_factory_impl<Socket_Options::O_DEBUG> {
            static auto setsockopt_(Platform_Socket handle, SockOptStatus b) { return setsockopt_O_DEBUG(handle, b); }
        };
        template <> struct setsockopt_factory_impl<Socket_Options::O_BROADCAST> {
            static auto setsockopt_(Platform_Socket handle, SockOptStatus b) { return setsockopt_O_BROADCAST(handle, b); }
        };
        template <> struct setsockopt_factory_impl<Socket_Options::O_REUSEADDR> {
            static auto setsockopt_(Platform_Socket handle, SockOptStatus b) { return setsockopt_O_REUSEADDR(handle, b); }
        };
        template <> struct setsockopt_factory_impl<Socket_Options::O_KEEPALIVE> {
            static auto setsockopt_(Platform_Socket handle, SockOptStatus b) { return setsockopt_O_KEEPALIVE(handle, b); }
        };
        template <> struct setsockopt_factory_impl<Socket_Options::O_LINGER> {
            static auto setsockopt_(Platform_Socket handle, Linger_Option b) { return setsockopt_O_LINGER(handle, b); }
        };
        template <> struct setsockopt_factory_impl<Socket_Options::O_OOBINLINE> {
            static auto setsockopt_(Platform_Socket handle, SockOptStatus b) { return setsockopt_O_OOBINLINE(handle, b); }
        };
        template <> struct setsockopt_factory_impl<Socket_Options::O_EXCLUSIVEADDRUSE> {
            static auto setsockopt_(Platform_Socket handle, SockOptStatus b) { return setsockopt_O_EXCLUSIVEADDRUSE(handle, b); }
        };
        template <> struct setsockopt_factory_impl<Socket_Options::O_SNDBUF> {
            static auto setsockopt_(Platform_Socket handle, int b) { return setsockopt_O_SNDBUF(handle, b); }
        };
        template <> struct setsockopt_factory_impl<Socket_Options::O_RCVBUF> {
            static auto setsockopt_(Platform_Socket handle, int b) { return setsockopt_O_RCVBUF(handle, b); }
        };
        template <> struct setsockopt_factory_impl<Socket_Options::O_RCVTIMEO> {
            static auto setsockopt_(Platform_Socket handle, std::chrono::seconds b) { return setsockopt_O_RCVTIMEO(handle, b); }
        };
        template <> struct setsockopt_factory_impl<Socket_Options::O_SNDTIMEO> {
            static auto setsockopt_(Platform_Socket handle, std::chrono::seconds b) { return setsockopt_O_SNDTIMEO(handle, b); }
        };
        template <> struct setsockopt_factory_impl<Socket_Options::O_NODELAY> {
            static auto setsockopt_(Platform_Socket handle, SockOptStatus b) { return setsockopt_O_NODELAY(handle, b); }
        };
        template <> struct setsockopt_factory_impl<Socket_Options::O_BLOCKING> {
            static auto setsockopt_(Platform_Socket handle, Blocking_Options b) { return setsockopt_O_BLOCKING(handle, b); }
        };

    } // namespace INTERNAL
    template <Socket_Options SO> auto getsockopt(Platform_Socket handle) { return INTERNAL::getsockopt_factory_impl<SO>::getsockopt_(handle); }
    template <Socket_Options SO, typename... Args> auto setsockopt(Platform_Socket handle, Args &&... args)
    {
        return INTERNAL::setsockopt_factory_impl<SO>::setsockopt_(handle, std::forward<Args>(args)...);
    }
    std::vector<sockaddr> SOCKET_LITE_EXTERN getaddrinfo(char *nodename, PortNumber pServiceName, Address_Family family);
    class IContext;
    class SOCKET_LITE_EXTERN ISocket : std::enable_shared_from_this<ISocket> {

      protected:
        Platform_Socket handle;

      public:
        ISocket();
        virtual ~ISocket() { close(); };
        template <Socket_Options SO> auto getsockopt() { return INTERNAL::getsockopt_factory_impl<SO>::getsockopt_(handle); }
        template <Socket_Options SO, typename... Args> auto setsockopt(Args &&... args)
        {
            return INTERNAL::setsockopt_factory_impl<SO>::setsockopt_(handle, std::forward<Args>(args)...);
        }
        std::optional<SL::NET::sockaddr> getpeername() const;
        std::optional<SL::NET::sockaddr> getsockname() const;
        bool bind(sockaddr addr);
        bool listen(int backlog) const;
        ConnectionAttemptStatus connect(SL::NET::sockaddr &addresses) const;
        virtual void connect(std::shared_ptr<IContext> &iocontext, SL::NET::sockaddr &address,
                             const std::function<void(ConnectionAttemptStatus)> &&) = 0;

        virtual void recv(size_t buffer_size, unsigned char *buffer, const std::function<void(Bytes_Transfered)> &&handler) = 0;
        virtual void send(size_t buffer_size, unsigned char *buffer, const std::function<void(Bytes_Transfered)> &&handler) = 0;
        virtual void close();
        Platform_Socket get_handle() const { return handle; }
        void set_handle(Platform_Socket h);
    };
    class SOCKET_LITE_EXTERN IContext {
      public:
        virtual ~IContext() {}
        virtual void run(ThreadCount threadcount) = 0;
        virtual std::shared_ptr<ISocket> CreateSocket() = 0;
    };
    std::shared_ptr<IContext> SOCKET_LITE_EXTERN CreateContext();

    class SOCKET_LITE_EXTERN IListener {
      public:
        virtual ~IListener() {}
        virtual void close() = 0;
        virtual void async_accept(const std::function<void(const std::shared_ptr<ISocket> &)> &&handler) = 0;
    };
    std::shared_ptr<IListener> SOCKET_LITE_EXTERN CreateListener(const std::shared_ptr<IContext> &iocontext,
                                                                 std::shared_ptr<ISocket> &&listensocket); // listener steals the socket

} // namespace NET
} // namespace SL
