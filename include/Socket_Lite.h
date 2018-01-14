#pragma once
#include <assert.h>
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
    enum class ConnectSelection { Selected, NotSelected };
    enum class ConnectionAttemptStatus { SuccessfullConnect, FailedConnect };
    enum class Linger_Options { LINGER_OFF, LINGER_ON };
    struct Linger_Option {
        Linger_Options l_onoff;        /* option on/off */
        std::chrono::seconds l_linger; /* linger time */
    };
    enum class Address_Family { IPV4, IPV6 };
    class SOCKET_LITE_EXTERN sockaddr {
        char SocketImpl[65];
        int SocketImplLen = 0;
        std::string Host;
        unsigned short Port = 0;
        Address_Family Family = Address_Family::IPV4;

      public:
        sockaddr(char *buffer, int len, char *host, unsigned short port, Address_Family family);
        const char *get_SocketAddr() const;
        int get_SocketAddrLen() const;
        std::string get_Host() const;
        unsigned short get_Port() const;
        Address_Family get_Family() const;
    };

    std::vector<sockaddr> SOCKET_LITE_EXTERN getaddrinfo(char *nodename, PortNumber pServiceName, Address_Family family);
    class SOCKET_LITE_EXTERN IIO_Context {
      public:
        virtual ~IIO_Context() {}
        virtual void run(ThreadCount threadcount) = 0;
    };
    std::shared_ptr<IIO_Context> SOCKET_LITE_EXTERN CreateIO_Context();

    class SOCKET_LITE_EXTERN ISocket : std::enable_shared_from_this<ISocket> {

        bool listen_(int backlog) const;
        bool bind_(sockaddr addr);
        std::optional<sockaddr> getpeername_() const;
        std::optional<sockaddr> getsockname_() const;
        std::optional<bool> getsockopt_O_DEBUG() const;
        std::optional<bool> getsockopt_O_ACCEPTCONN() const;
        std::optional<bool> getsockopt_O_BROADCAST() const;
        std::optional<bool> getsockopt_O_REUSEADDR() const;
        std::optional<bool> getsockopt_O_KEEPALIVE() const;
        std::optional<Linger_Option> getsockopt_O_LINGER() const;
        std::optional<bool> getsockopt_O_OOBINLINE() const;
        std::optional<bool> getsockopt_O_EXCLUSIVEADDRUSE() const;
        std::optional<int> getsockopt_O_SNDBUF() const;
        std::optional<int> getsockopt_O_RCVBUF() const;
        std::optional<std::chrono::seconds> getsockopt_O_SNDTIMEO() const;
        std::optional<std::chrono::seconds> getsockopt_O_RCVTIMEO() const;
        std::optional<int> getsockopt_O_ERROR() const;
        std::optional<bool> getsockopt_O_NODELAY() const;
        std::optional<Blocking_Options> getsockopt_O_BLOCKING() const;

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

        template <Socket_Options SO> struct getsockopt_factory_impl;
        template <> struct getsockopt_factory_impl<Socket_Options::O_DEBUG> {
            auto getsockopt_(ISocket *s) { return s->getsockopt_O_DEBUG(); }
        };
        template <> struct getsockopt_factory_impl<Socket_Options::O_ACCEPTCONN> {
            auto getsockopt_(ISocket *s) { return s->getsockopt_O_ACCEPTCONN(); }
        };
        template <> struct getsockopt_factory_impl<Socket_Options::O_BROADCAST> {
            auto getsockopt_(ISocket *s) { return s->getsockopt_O_BROADCAST(); }
        };
        template <> struct getsockopt_factory_impl<Socket_Options::O_REUSEADDR> {
            auto getsockopt_(ISocket *s) { return s->getsockopt_O_REUSEADDR(); }
        };
        template <> struct getsockopt_factory_impl<Socket_Options::O_KEEPALIVE> {
            auto getsockopt_(ISocket *s) { return s->getsockopt_O_KEEPALIVE(); }
        };
        template <> struct getsockopt_factory_impl<Socket_Options::O_LINGER> {
            auto getsockopt_(ISocket *s) { return s->getsockopt_O_LINGER(); }
        };
        template <> struct getsockopt_factory_impl<Socket_Options::O_OOBINLINE> {
            auto getsockopt_(ISocket *s) { return s->getsockopt_O_OOBINLINE(); }
        };
        template <> struct getsockopt_factory_impl<Socket_Options::O_EXCLUSIVEADDRUSE> {
            auto getsockopt_(ISocket *s) { return s->getsockopt_O_EXCLUSIVEADDRUSE(); }
        };
        template <> struct getsockopt_factory_impl<Socket_Options::O_SNDBUF> {
            auto getsockopt_(ISocket *s) { return s->getsockopt_O_SNDBUF(); }
        };
        template <> struct getsockopt_factory_impl<Socket_Options::O_RCVBUF> {
            auto getsockopt_(ISocket *s) { return s->getsockopt_O_RCVBUF(); }
        };
        template <> struct getsockopt_factory_impl<Socket_Options::O_RCVTIMEO> {
            auto getsockopt_(ISocket *s) { return s->getsockopt_O_RCVTIMEO(); }
        };
        template <> struct getsockopt_factory_impl<Socket_Options::O_SNDTIMEO> {
            auto getsockopt_(ISocket *s) { return s->getsockopt_O_SNDTIMEO(); }
        };
        template <> struct getsockopt_factory_impl<Socket_Options::O_ERROR> {
            auto getsockopt_(ISocket *s) { return s->getsockopt_O_ERROR(); }
        };
        template <> struct getsockopt_factory_impl<Socket_Options::O_NODELAY> {
            auto getsockopt_(ISocket *s) { return s->getsockopt_O_NODELAY(); }
        };
        template <> struct getsockopt_factory_impl<Socket_Options::O_BLOCKING> {
            auto getsockopt_(ISocket *s) { return s->getsockopt_O_BLOCKING(); }
        };

        template <Socket_Options SO> struct setsockopt_factory_impl;
        template <> struct setsockopt_factory_impl<Socket_Options::O_DEBUG> {
            auto setsockopt_(ISocket *s, bool b) { return s->setsockopt_O_DEBUG(b); }
        };
        template <> struct setsockopt_factory_impl<Socket_Options::O_BROADCAST> {
            auto setsockopt_(ISocket *s, bool b) { return s->setsockopt_O_BROADCAST(b); }
        };
        template <> struct setsockopt_factory_impl<Socket_Options::O_REUSEADDR> {
            auto setsockopt_(ISocket *s, bool b) { return s->setsockopt_O_REUSEADDR(b); }
        };
        template <> struct setsockopt_factory_impl<Socket_Options::O_KEEPALIVE> {
            auto setsockopt_(ISocket *s, bool b) { return s->setsockopt_O_KEEPALIVE(b); }
        };
        template <> struct setsockopt_factory_impl<Socket_Options::O_LINGER> {
            auto setsockopt_(ISocket *s, Linger_Option b) { return s->setsockopt_O_LINGER(b); }
        };
        template <> struct setsockopt_factory_impl<Socket_Options::O_OOBINLINE> {
            auto setsockopt_(ISocket *s, bool b) { return s->setsockopt_O_OOBINLINE(b); }
        };
        template <> struct setsockopt_factory_impl<Socket_Options::O_EXCLUSIVEADDRUSE> {
            auto setsockopt_(ISocket *s, bool b) { return s->setsockopt_O_EXCLUSIVEADDRUSE(b); }
        };
        template <> struct setsockopt_factory_impl<Socket_Options::O_SNDBUF> {
            auto setsockopt_(ISocket *s, int b) { return s->setsockopt_O_SNDBUF(b); }
        };
        template <> struct setsockopt_factory_impl<Socket_Options::O_RCVBUF> {
            auto setsockopt_(ISocket *s, int b) { return s->setsockopt_O_RCVBUF(b); }
        };
        template <> struct setsockopt_factory_impl<Socket_Options::O_RCVTIMEO> {
            auto setsockopt_(ISocket *s, std::chrono::seconds b) { return s->setsockopt_O_RCVTIMEO(b); }
        };
        template <> struct setsockopt_factory_impl<Socket_Options::O_SNDTIMEO> {
            auto setsockopt_(ISocket *s, std::chrono::seconds b) { return s->setsockopt_O_SNDTIMEO(b); }
        };
        template <> struct setsockopt_factory_impl<Socket_Options::O_NODELAY> {
            auto setsockopt_(ISocket *s, bool b) { return s->setsockopt_O_NODELAY(b); }
        };
        template <> struct setsockopt_factory_impl<Socket_Options::O_BLOCKING> {
            auto setsockopt_(ISocket *s, Blocking_Options b) { return s->setsockopt_O_BLOCKING(b); }
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
        auto getpeername() const { return getpeername_(); }
        auto getsockname() const { return getsockname_(); }
        auto bind(sockaddr addr) { return bind_(addr); }
        auto listen(int backlog) const { return listen_(backlog); }

        virtual void async_connect(const std::shared_ptr<IIO_Context> &io_context, std::vector<SL::NET::sockaddr> &addresses,
                                   const std::function<ConnectSelection(ConnectionAttemptStatus, SL::NET::sockaddr &)> &&) = 0;
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