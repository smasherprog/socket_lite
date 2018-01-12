#pragma once
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
#ifdef WIN32
    typedef unsigned int Platform_Socket;
#else
    typedef int Platform_Socket;
#endif

    typedef Explicit<unsigned short, INTERNAL::ThreadCountTag> ThreadCount;
    typedef Explicit<unsigned short, INTERNAL::PorNumbertTag> PortNumber;
    enum class Linger_Options { LINGER_OFF, LINGER_ON };
    struct Linger_Option {
        Linger_Options l_onoff;        /* option on/off */
        std::chrono::seconds l_linger; /* linger time */
    };
    enum class Address_Family { IPV4, IPV6 };
    struct SOCKET_LITE_EXTERN sockaddr {
        char Address[65] = {0};
        unsigned short Port = 0;
        Address_Family Family = Address_Family::IPV4;
    };

    std::vector<sockaddr> SOCKET_LITE_EXTERN getaddrinfo(char *nodename, PortNumber pServiceName, Address_Family family);
    class SOCKET_LITE_EXTERN IIO_Context {
      public:
        virtual ~IIO_Context() {}
        virtual void run(ThreadCount threadcount) = 0;
    };
    std::shared_ptr<IIO_Context> SOCKET_LITE_EXTERN CreateIO_Context();

    class SOCKET_LITE_EXTERN ISocket : std::enable_shared_from_this<ISocket> {

        bool listen_(Platform_Socket s, int backlog) const;
        bool bind_(Platform_Socket s, sockaddr addr) const;
        std::optional<sockaddr> getpeername_(Platform_Socket s) const;
        std::optional<sockaddr> getsockname_(Platform_Socket s) const;
        std::optional<bool> getsockopt_O_DEBUG(Platform_Socket s) const;
        std::optional<bool> getsockopt_O_ACCEPTCONN(Platform_Socket s) const;
        std::optional<bool> getsockopt_O_BROADCAST(Platform_Socket s) const;
        std::optional<bool> getsockopt_O_REUSEADDR(Platform_Socket s) const;
        std::optional<bool> getsockopt_O_KEEPALIVE(Platform_Socket s) const;
        std::optional<Linger_Option> getsockopt_O_LINGER(Platform_Socket s) const;
        std::optional<bool> getsockopt_O_OOBINLINE(Platform_Socket s) const;
        std::optional<bool> getsockopt_O_EXCLUSIVEADDRUSE(Platform_Socket s) const;
        std::optional<int> getsockopt_O_SNDBUF(Platform_Socket s) const;
        std::optional<int> getsockopt_O_RCVBUF(Platform_Socket s) const;
        std::optional<std::chrono::seconds> getsockopt_O_SNDTIMEO(Platform_Socket s) const;
        std::optional<std::chrono::seconds> getsockopt_O_RCVTIMEO(Platform_Socket s) const;
        std::optional<int> getsockopt_O_ERROR(Platform_Socket s) const;
        std::optional<bool> getsockopt_O_NODELAY(Platform_Socket s) const;
        std::optional<Blocking_Options> getsockopt_O_BLOCKING(Platform_Socket s) const;

        bool setsockopt_O_DEBUG(Platform_Socket s, bool b) const;
        bool setsockopt_O_BROADCAST(Platform_Socket s, bool b) const;
        bool setsockopt_O_REUSEADDR(Platform_Socket s, bool b) const;
        bool setsockopt_O_KEEPALIVE(Platform_Socket s, bool b) const;
        bool setsockopt_O_LINGER(Platform_Socket s, Linger_Option o) const;
        bool setsockopt_O_OOBINLINE(Platform_Socket s, bool b) const;
        bool setsockopt_O_EXCLUSIVEADDRUSE(Platform_Socket s, bool b) const;
        bool setsockopt_O_SNDBUF(Platform_Socket s, int b) const;
        bool setsockopt_O_RCVBUF(Platform_Socket s, int b) const;
        bool setsockopt_O_SNDTIMEO(Platform_Socket s, std::chrono::seconds sec) const;
        bool setsockopt_O_RCVTIMEO(Platform_Socket s, std::chrono::seconds sec) const;
        bool setsockopt_O_NODELAY(Platform_Socket s, bool b) const;
        bool setsockopt_O_BLOCKING(Platform_Socket s, Blocking_Options b) const;

        template <Socket_Options SO> struct getsockopt_factory_impl;
        template <> struct getsockopt_factory_impl<Socket_Options::O_DEBUG> {
            auto getsockopt_(Platform_Socket handle, ISocket *s) { return s->getsockopt_O_DEBUG(handle); }
        };
        template <> struct getsockopt_factory_impl<Socket_Options::O_ACCEPTCONN> {
            auto getsockopt_(Platform_Socket handle, ISocket *s) { return s->getsockopt_O_ACCEPTCONN(handle); }
        };
        template <> struct getsockopt_factory_impl<Socket_Options::O_BROADCAST> {
            auto getsockopt_(Platform_Socket handle, ISocket *s) { return s->getsockopt_O_BROADCAST(handle); }
        };
        template <> struct getsockopt_factory_impl<Socket_Options::O_REUSEADDR> {
            auto getsockopt_(Platform_Socket handle, ISocket *s) { return s->getsockopt_O_REUSEADDR(handle); }
        };
        template <> struct getsockopt_factory_impl<Socket_Options::O_KEEPALIVE> {
            auto getsockopt_(Platform_Socket handle, ISocket *s) { return s->getsockopt_O_KEEPALIVE(handle); }
        };
        template <> struct getsockopt_factory_impl<Socket_Options::O_LINGER> {
            auto getsockopt_(Platform_Socket handle, ISocket *s) { return s->getsockopt_O_LINGER(handle); }
        };
        template <> struct getsockopt_factory_impl<Socket_Options::O_OOBINLINE> {
            auto getsockopt_(Platform_Socket handle, ISocket *s) { return s->getsockopt_O_OOBINLINE(handle); }
        };
        template <> struct getsockopt_factory_impl<Socket_Options::O_EXCLUSIVEADDRUSE> {
            auto getsockopt_(Platform_Socket handle, ISocket *s) { return s->getsockopt_O_EXCLUSIVEADDRUSE(handle); }
        };
        template <> struct getsockopt_factory_impl<Socket_Options::O_SNDBUF> {
            auto getsockopt_(Platform_Socket handle, ISocket *s) { return s->getsockopt_O_SNDBUF(handle); }
        };
        template <> struct getsockopt_factory_impl<Socket_Options::O_RCVBUF> {
            auto getsockopt_(Platform_Socket handle, ISocket *s) { return s->getsockopt_O_RCVBUF(handle); }
        };
        template <> struct getsockopt_factory_impl<Socket_Options::O_RCVTIMEO> {
            auto getsockopt_(Platform_Socket handle, ISocket *s) { return s->getsockopt_O_RCVTIMEO(handle); }
        };
        template <> struct getsockopt_factory_impl<Socket_Options::O_SNDTIMEO> {
            auto getsockopt_(Platform_Socket handle, ISocket *s) { return s->getsockopt_O_SNDTIMEO(handle); }
        };
        template <> struct getsockopt_factory_impl<Socket_Options::O_ERROR> {
            auto getsockopt_(Platform_Socket handle, ISocket *s) { return s->getsockopt_O_ERROR(handle); }
        };
        template <> struct getsockopt_factory_impl<Socket_Options::O_NODELAY> {
            auto getsockopt_(Platform_Socket handle, ISocket *s) { return s->getsockopt_O_NODELAY(handle); }
        };
        template <> struct getsockopt_factory_impl<Socket_Options::O_BLOCKING> {
            auto getsockopt_(Platform_Socket handle, ISocket *s) { return s->getsockopt_O_BLOCKING(handle); }
        };

        template <Socket_Options SO> struct setsockopt_factory_impl;
        template <> struct setsockopt_factory_impl<Socket_Options::O_DEBUG> {
            auto setsockopt_(Platform_Socket handle, ISocket *s, bool b) { return s->setsockopt_O_DEBUG(handle, b); }
        };
        template <> struct setsockopt_factory_impl<Socket_Options::O_BROADCAST> {
            auto setsockopt_(Platform_Socket handle, ISocket *s, bool b) { return s->setsockopt_O_BROADCAST(handle, b); }
        };
        template <> struct setsockopt_factory_impl<Socket_Options::O_REUSEADDR> {
            auto setsockopt_(Platform_Socket handle, ISocket *s, bool b) { return s->setsockopt_O_REUSEADDR(handle, b); }
        };
        template <> struct setsockopt_factory_impl<Socket_Options::O_KEEPALIVE> {
            auto setsockopt_(Platform_Socket handle, ISocket *s, bool b) { return s->setsockopt_O_KEEPALIVE(handle, b); }
        };
        template <> struct setsockopt_factory_impl<Socket_Options::O_LINGER> {
            auto setsockopt_(Platform_Socket handle, ISocket *s, Linger_Option b) { return s->setsockopt_O_LINGER(handle, b); }
        };
        template <> struct setsockopt_factory_impl<Socket_Options::O_OOBINLINE> {
            auto setsockopt_(Platform_Socket handle, ISocket *s, bool b) { return s->setsockopt_O_OOBINLINE(handle, b); }
        };
        template <> struct setsockopt_factory_impl<Socket_Options::O_EXCLUSIVEADDRUSE> {
            auto setsockopt_(Platform_Socket handle, ISocket *s, bool b) { return s->setsockopt_O_EXCLUSIVEADDRUSE(handle, b); }
        };
        template <> struct setsockopt_factory_impl<Socket_Options::O_SNDBUF> {
            auto setsockopt_(Platform_Socket handle, ISocket *s, int b) { return s->setsockopt_O_SNDBUF(handle, b); }
        };
        template <> struct setsockopt_factory_impl<Socket_Options::O_RCVBUF> {
            auto setsockopt_(Platform_Socket handle, ISocket *s, int b) { return s->setsockopt_O_RCVBUF(handle, b); }
        };
        template <> struct setsockopt_factory_impl<Socket_Options::O_RCVTIMEO> {
            auto setsockopt_(Platform_Socket handle, ISocket *s, std::chrono::seconds b) { return s->setsockopt_O_RCVTIMEO(handle, b); }
        };
        template <> struct setsockopt_factory_impl<Socket_Options::O_SNDTIMEO> {
            auto setsockopt_(Platform_Socket handle, ISocket *s, std::chrono::seconds b) { return s->setsockopt_O_SNDTIMEO(handle, b); }
        };
        template <> struct setsockopt_factory_impl<Socket_Options::O_NODELAY> {
            auto setsockopt_(Platform_Socket handle, ISocket *s, bool b) { return s->setsockopt_O_NODELAY(handle, b); }
        };
        template <> struct setsockopt_factory_impl<Socket_Options::O_BLOCKING> {
            auto setsockopt_(Platform_Socket handle, ISocket *s, Blocking_Options b) { return s->setsockopt_O_BLOCKING(handle, b); }
        };

      protected:
        Platform_Socket handle;

      public:
        ISocket() {}
        virtual ~ISocket(){};
        template <Socket_Options SO> auto getsockopt() const { return getsockopt_factory_impl<SO>::getsockopt_(handle, this); }
        template <Socket_Options SO, typename... Args> auto setsockopt(Args &&... args)
        {
            return getsockopt_factory_impl<SO>::setsockopt_(this, std::forward<Args>(args)...);
        }
        auto getpeername() const { return getpeername_(handle); }
        auto getsockname() const { return getsockname_(handle); }
        auto bind(sockaddr addr) const { return bind_(handle, addr); }
        auto listen(int backlog) const { return listen_(handle, backlog); }

        virtual void async_connect(const std::shared_ptr<IIO_Context> &io_context, std::vector<SL::NET::sockaddr> &addresses,
                                   const std::function<bool(bool, SL::NET::sockaddr &)> &&) = 0;
        virtual void async_read(size_t buffer_size, unsigned char *buffer, const std::function<void(Bytes_Transfered)> &&handler) = 0;
        virtual void async_write(size_t buffer_size, unsigned char *buffer, const std::function<void(Bytes_Transfered)> &&handler) = 0;
        // send a close message and close the socket
        virtual void close() = 0;
    };

    std::shared_ptr<ISocket> SOCKET_LITE_EXTERN CreateSocket(const std::shared_ptr<IIO_Context> &iocontext);

    class SOCKET_LITE_EXTERN IListener {
      public:
        virtual ~IListener() {}
        virtual void async_accept(std::shared_ptr<ISocket> &socket, const std::function<void(bool)> &&handler) = 0;
    };
    std::shared_ptr<IListener> SOCKET_LITE_EXTERN CreateListener(const std::shared_ptr<IIO_Context> &iocontext,
                                                                 std::shared_ptr<ISocket> &&listensocket); // listener steals the socket

} // namespace NET
} // namespace SL