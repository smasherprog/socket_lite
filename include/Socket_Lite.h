#pragma once

#include <chrono>
#include <functional>
#include <memory>
#include <optional>
#include <stdint.h>
#include <tuple>
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

    enum class SocketOptions {
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
#ifdef _WIN32
#if ((UINTPTR_MAX) == (UINT_MAX))
    typedef unsigned int PlatformSocket;
#else

    typedef unsigned long long PlatformSocket;
#endif
#else
    typedef int PlatformSocket;
#endif

    typedef Explicit<unsigned short, INTERNAL::ThreadCountTag> ThreadCount;
    typedef Explicit<unsigned short, INTERNAL::PorNumbertTag> PortNumber;

    enum StatusCode {
        SC_EAGAIN = INT_MIN,
        SC_EWOULDBLOCK,
        SC_EBADF,
        SC_ECONNRESET,
        SC_EINTR,
        SC_EINVAL,
        SC_ENOTCONN,
        SC_ENOTSOCK,
        SC_EOPNOTSUPP,
        SC_ETIMEDOUT,
        SC_CLOSED,
        SC_SUCCESS = 0
    };
    enum class LingerOptions { LINGER_OFF, LINGER_ON };
    enum class SockOptStatus { ENABLED, DISABLED };
    struct LingerOption {
        LingerOptions l_onoff;         /* option on/off */
        std::chrono::seconds l_linger; /* linger time */
    };
    enum class AddressFamily { IPV4, IPV6 };
    class SOCKET_LITE_EXTERN sockaddr {
        unsigned char SocketImpl[65] = {0};
        int SocketImplLen = 0;
        std::string Host;
        unsigned short Port = 0;
        AddressFamily Family = AddressFamily::IPV4;

      public:
        sockaddr() {}
        sockaddr(unsigned char *buffer, int len, char *host, unsigned short port, AddressFamily family);
        sockaddr(const sockaddr &addr);
        const unsigned char *get_SocketAddr() const;
        int get_SocketAddrLen() const;
        std::string get_Host() const;
        unsigned short get_Port() const;
        AddressFamily get_Family() const;
    };
    namespace INTERNAL {
        std::tuple<StatusCode, std::optional<SockOptStatus>> SOCKET_LITE_EXTERN getsockopt_O_DEBUG(PlatformSocket handle);
        std::tuple<StatusCode, std::optional<SockOptStatus>> SOCKET_LITE_EXTERN getsockopt_O_ACCEPTCONN(PlatformSocket handle);
        std::tuple<StatusCode, std::optional<SockOptStatus>> SOCKET_LITE_EXTERN getsockopt_O_BROADCAST(PlatformSocket handle);
        std::tuple<StatusCode, std::optional<SockOptStatus>> SOCKET_LITE_EXTERN getsockopt_O_REUSEADDR(PlatformSocket handle);
        std::tuple<StatusCode, std::optional<SockOptStatus>> SOCKET_LITE_EXTERN getsockopt_O_KEEPALIVE(PlatformSocket handle);
        std::tuple<StatusCode, std::optional<LingerOption>> SOCKET_LITE_EXTERN getsockopt_O_LINGER(PlatformSocket handle);
        std::tuple<StatusCode, std::optional<SockOptStatus>> SOCKET_LITE_EXTERN getsockopt_O_OOBINLINE(PlatformSocket handle);
        std::tuple<StatusCode, std::optional<SockOptStatus>> SOCKET_LITE_EXTERN getsockopt_O_EXCLUSIVEADDRUSE(PlatformSocket handle);
        std::tuple<StatusCode, std::optional<int>> SOCKET_LITE_EXTERN getsockopt_O_SNDBUF(PlatformSocket handle);
        std::tuple<StatusCode, std::optional<int>> SOCKET_LITE_EXTERN getsockopt_O_RCVBUF(PlatformSocket handle);
        std::tuple<StatusCode, std::optional<std::chrono::seconds>> SOCKET_LITE_EXTERN getsockopt_O_SNDTIMEO(PlatformSocket handle);
        std::tuple<StatusCode, std::optional<std::chrono::seconds>> SOCKET_LITE_EXTERN getsockopt_O_RCVTIMEO(PlatformSocket handle);
        std::tuple<StatusCode, std::optional<int>> SOCKET_LITE_EXTERN getsockopt_O_ERROR(PlatformSocket handle);
        std::tuple<StatusCode, std::optional<SockOptStatus>> SOCKET_LITE_EXTERN getsockopt_O_NODELAY(PlatformSocket handle);
        std::tuple<StatusCode, std::optional<Blocking_Options>> SOCKET_LITE_EXTERN getsockopt_O_BLOCKING(PlatformSocket handle);

        template <SocketOptions SO> struct getsockopt_factory_impl;
        template <> struct getsockopt_factory_impl<SocketOptions::O_DEBUG> {
            static auto getsockopt_(PlatformSocket handle) { return getsockopt_O_DEBUG(handle); }
        };
        template <> struct getsockopt_factory_impl<SocketOptions::O_ACCEPTCONN> {
            static auto getsockopt_(PlatformSocket handle) { return getsockopt_O_ACCEPTCONN(handle); }
        };
        template <> struct getsockopt_factory_impl<SocketOptions::O_BROADCAST> {
            static auto getsockopt_(PlatformSocket handle) { return getsockopt_O_BROADCAST(handle); }
        };
        template <> struct getsockopt_factory_impl<SocketOptions::O_REUSEADDR> {
            static auto getsockopt_(PlatformSocket handle) { return getsockopt_O_REUSEADDR(handle); }
        };
        template <> struct getsockopt_factory_impl<SocketOptions::O_KEEPALIVE> {
            static auto getsockopt_(PlatformSocket handle) { return getsockopt_O_KEEPALIVE(handle); }
        };
        template <> struct getsockopt_factory_impl<SocketOptions::O_LINGER> {
            static auto getsockopt_(PlatformSocket handle) { return getsockopt_O_LINGER(handle); }
        };
        template <> struct getsockopt_factory_impl<SocketOptions::O_OOBINLINE> {
            static auto getsockopt_(PlatformSocket handle) { return getsockopt_O_OOBINLINE(handle); }
        };
        template <> struct getsockopt_factory_impl<SocketOptions::O_EXCLUSIVEADDRUSE> {
            static auto getsockopt_(PlatformSocket handle) { return getsockopt_O_EXCLUSIVEADDRUSE(handle); }
        };
        template <> struct getsockopt_factory_impl<SocketOptions::O_SNDBUF> {
            static auto getsockopt_(PlatformSocket handle) { return getsockopt_O_SNDBUF(handle); }
        };
        template <> struct getsockopt_factory_impl<SocketOptions::O_RCVBUF> {
            static auto getsockopt_(PlatformSocket handle) { return getsockopt_O_RCVBUF(handle); }
        };
        template <> struct getsockopt_factory_impl<SocketOptions::O_RCVTIMEO> {
            static auto getsockopt_(PlatformSocket handle) { return getsockopt_O_RCVTIMEO(handle); }
        };
        template <> struct getsockopt_factory_impl<SocketOptions::O_SNDTIMEO> {
            static auto getsockopt_(PlatformSocket handle) { return getsockopt_O_SNDTIMEO(handle); }
        };
        template <> struct getsockopt_factory_impl<SocketOptions::O_ERROR> {
            static auto getsockopt_(PlatformSocket handle) { return getsockopt_O_ERROR(handle); }
        };
        template <> struct getsockopt_factory_impl<SocketOptions::O_NODELAY> {
            static auto getsockopt_(PlatformSocket handle) { return getsockopt_O_NODELAY(handle); }
        };
        template <> struct getsockopt_factory_impl<SocketOptions::O_BLOCKING> {
            static auto getsockopt_(PlatformSocket handle) { return getsockopt_O_BLOCKING(handle); }
        };

        bool SOCKET_LITE_EXTERN setsockopt_O_DEBUG(PlatformSocket handle, SockOptStatus b);
        bool SOCKET_LITE_EXTERN setsockopt_O_BROADCAST(PlatformSocket handle, SockOptStatus b);
        bool SOCKET_LITE_EXTERN setsockopt_O_REUSEADDR(PlatformSocket handle, SockOptStatus b);
        bool SOCKET_LITE_EXTERN setsockopt_O_KEEPALIVE(PlatformSocket handle, SockOptStatus b);
        bool SOCKET_LITE_EXTERN setsockopt_O_LINGER(PlatformSocket handle, LingerOption o);
        bool SOCKET_LITE_EXTERN setsockopt_O_OOBINLINE(PlatformSocket handle, SockOptStatus b);
        bool SOCKET_LITE_EXTERN setsockopt_O_EXCLUSIVEADDRUSE(PlatformSocket handle, SockOptStatus b);
        bool SOCKET_LITE_EXTERN setsockopt_O_SNDBUF(PlatformSocket handle, int b);
        bool SOCKET_LITE_EXTERN setsockopt_O_RCVBUF(PlatformSocket handle, int b);
        bool SOCKET_LITE_EXTERN setsockopt_O_SNDTIMEO(PlatformSocket handle, std::chrono::seconds sec);
        bool SOCKET_LITE_EXTERN setsockopt_O_RCVTIMEO(PlatformSocket handle, std::chrono::seconds sec);
        bool SOCKET_LITE_EXTERN setsockopt_O_NODELAY(PlatformSocket handle, SockOptStatus b);
        bool SOCKET_LITE_EXTERN setsockopt_O_BLOCKING(PlatformSocket handle, Blocking_Options b);

        template <SocketOptions SO> struct setsockopt_factory_impl;
        template <> struct setsockopt_factory_impl<SocketOptions::O_DEBUG> {
            static auto setsockopt_(PlatformSocket handle, SockOptStatus b) { return setsockopt_O_DEBUG(handle, b); }
        };
        template <> struct setsockopt_factory_impl<SocketOptions::O_BROADCAST> {
            static auto setsockopt_(PlatformSocket handle, SockOptStatus b) { return setsockopt_O_BROADCAST(handle, b); }
        };
        template <> struct setsockopt_factory_impl<SocketOptions::O_REUSEADDR> {
            static auto setsockopt_(PlatformSocket handle, SockOptStatus b) { return setsockopt_O_REUSEADDR(handle, b); }
        };
        template <> struct setsockopt_factory_impl<SocketOptions::O_KEEPALIVE> {
            static auto setsockopt_(PlatformSocket handle, SockOptStatus b) { return setsockopt_O_KEEPALIVE(handle, b); }
        };
        template <> struct setsockopt_factory_impl<SocketOptions::O_LINGER> {
            static auto setsockopt_(PlatformSocket handle, LingerOption b) { return setsockopt_O_LINGER(handle, b); }
        };
        template <> struct setsockopt_factory_impl<SocketOptions::O_OOBINLINE> {
            static auto setsockopt_(PlatformSocket handle, SockOptStatus b) { return setsockopt_O_OOBINLINE(handle, b); }
        };
        template <> struct setsockopt_factory_impl<SocketOptions::O_EXCLUSIVEADDRUSE> {
            static auto setsockopt_(PlatformSocket handle, SockOptStatus b) { return setsockopt_O_EXCLUSIVEADDRUSE(handle, b); }
        };
        template <> struct setsockopt_factory_impl<SocketOptions::O_SNDBUF> {
            static auto setsockopt_(PlatformSocket handle, int b) { return setsockopt_O_SNDBUF(handle, b); }
        };
        template <> struct setsockopt_factory_impl<SocketOptions::O_RCVBUF> {
            static auto setsockopt_(PlatformSocket handle, int b) { return setsockopt_O_RCVBUF(handle, b); }
        };
        template <> struct setsockopt_factory_impl<SocketOptions::O_RCVTIMEO> {
            static auto setsockopt_(PlatformSocket handle, std::chrono::seconds b) { return setsockopt_O_RCVTIMEO(handle, b); }
        };
        template <> struct setsockopt_factory_impl<SocketOptions::O_SNDTIMEO> {
            static auto setsockopt_(PlatformSocket handle, std::chrono::seconds b) { return setsockopt_O_SNDTIMEO(handle, b); }
        };
        template <> struct setsockopt_factory_impl<SocketOptions::O_NODELAY> {
            static auto setsockopt_(PlatformSocket handle, SockOptStatus b) { return setsockopt_O_NODELAY(handle, b); }
        };
        template <> struct setsockopt_factory_impl<SocketOptions::O_BLOCKING> {
            static auto setsockopt_(PlatformSocket handle, Blocking_Options b) { return setsockopt_O_BLOCKING(handle, b); }
        };

    } // namespace INTERNAL
    template <SocketOptions SO> auto getsockopt(PlatformSocket handle) { return INTERNAL::getsockopt_factory_impl<SO>::getsockopt_(handle); }
    template <SocketOptions SO, typename... Args> auto setsockopt(PlatformSocket handle, Args &&... args)
    {
        return INTERNAL::setsockopt_factory_impl<SO>::setsockopt_(handle, std::forward<Args>(args)...);
    }
    std::tuple<StatusCode, std::vector<sockaddr>> SOCKET_LITE_EXTERN getaddrinfo(char *nodename, PortNumber pServiceName, AddressFamily family);
    class IContext;
    class SOCKET_LITE_EXTERN ISocket {

      protected:
        PlatformSocket handle;

      public:
        ISocket();
        virtual ~ISocket() { close(); };
        template <SocketOptions SO> auto getsockopt() { return INTERNAL::getsockopt_factory_impl<SO>::getsockopt_(handle); }
        template <SocketOptions SO, typename... Args> auto setsockopt(Args &&... args)
        {
            return INTERNAL::setsockopt_factory_impl<SO>::setsockopt_(handle, std::forward<Args>(args)...);
        }
        std::optional<SL::NET::sockaddr> getpeername() const;
        std::optional<SL::NET::sockaddr> getsockname() const;
        StatusCode bind(sockaddr addr);
        StatusCode listen(int backlog) const;
        StatusCode connect(SL::NET::sockaddr &addresses) const;
        virtual void connect(SL::NET::sockaddr &address, const std::function<void(StatusCode)> &&) = 0;

        virtual void recv(size_t buffer_size, unsigned char *buffer, const std::function<void(StatusCode, size_t)> &&handler) = 0;
        virtual void send(size_t buffer_size, unsigned char *buffer, const std::function<void(StatusCode, size_t)> &&handler) = 0;
        virtual void close();
        PlatformSocket get_handle() const { return handle; }
        void set_handle(PlatformSocket h);
    };
    class SOCKET_LITE_EXTERN IListener {
      public:
        virtual ~IListener() {}
        virtual void close() = 0;
        virtual void async_accept(const std::function<void(StatusCode, const std::shared_ptr<ISocket> &)> &&handler) = 0;
    };
    class SOCKET_LITE_EXTERN IContext {
      public:
        virtual ~IContext() {}
        virtual void run(ThreadCount threadcount) = 0;
        virtual std::shared_ptr<ISocket> CreateSocket() = 0;
        virtual std::shared_ptr<IListener> CreateListener(std::shared_ptr<ISocket> &&listensocket) = 0; // listener steals the socket
    };
    std::shared_ptr<IContext> SOCKET_LITE_EXTERN CreateContext();

} // namespace NET
} // namespace SL
