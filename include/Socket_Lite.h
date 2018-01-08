#pragma once
#include <chrono>
#include <functional>
#include <memory>
#include <optional>
#include <stdint.h>
#include <variant>

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

    struct NameInfo {
        char HostName[1025];
        char Port[32];
    };

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
        O_NODELAY
    };
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
    struct SocketInfo {
        char Address[65];
        unsigned short Port;
        Address_Family Family;
    };
    std::optional<SocketInfo> SOCKET_LITE_EXTERN get_PeerInfo(Platform_Socket s);
    std::optional<Platform_Socket> SOCKET_LITE_EXTERN create_and_bind(PortNumber port);
    bool SOCKET_LITE_EXTERN make_socket_non_blocking(Platform_Socket socket);
    std::optional<Platform_Socket> SOCKET_LITE_EXTERN create_and_connect(std::string host, PortNumber port
#ifdef WIN32
                                                                         ,
                                                                         void **iocphandle, void *completionkey, void *overlappeddata
#endif
    );

    std::optional<bool> SOCKET_LITE_EXTERN getsockopt_O_DEBUG(Platform_Socket s);
    std::optional<bool> SOCKET_LITE_EXTERN getsockopt_O_ACCEPTCONN(Platform_Socket s);
    std::optional<bool> SOCKET_LITE_EXTERN getsockopt_O_BROADCAST(Platform_Socket s);
    std::optional<bool> SOCKET_LITE_EXTERN getsockopt_O_REUSEADDR(Platform_Socket s);
    std::optional<bool> SOCKET_LITE_EXTERN getsockopt_O_KEEPALIVE(Platform_Socket s);
    std::optional<Linger_Option> SOCKET_LITE_EXTERN getsockopt_O_LINGER(Platform_Socket s);
    std::optional<bool> SOCKET_LITE_EXTERN getsockopt_O_OOBINLINE(Platform_Socket s);
    std::optional<bool> SOCKET_LITE_EXTERN getsockopt_O_EXCLUSIVEADDRUSE(Platform_Socket s);
    std::optional<int> SOCKET_LITE_EXTERN getsockopt_O_SNDBUF(Platform_Socket s);
    std::optional<int> SOCKET_LITE_EXTERN getsockopt_O_RCVBUF(Platform_Socket s);
    std::optional<std::chrono::seconds> SOCKET_LITE_EXTERN getsockopt_O_SNDTIMEO(Platform_Socket s);
    std::optional<std::chrono::seconds> SOCKET_LITE_EXTERN getsockopt_O_RCVTIMEO(Platform_Socket s);
    std::optional<int> SOCKET_LITE_EXTERN getsockopt_O_ERROR(Platform_Socket s);
    std::optional<bool> SOCKET_LITE_EXTERN getsockopt_O_NODELAY(Platform_Socket s);

    bool SOCKET_LITE_EXTERN setsockopt_O_DEBUG(Platform_Socket s, bool b);
    bool SOCKET_LITE_EXTERN setsockopt_O_BROADCAST(Platform_Socket s, bool b);
    bool SOCKET_LITE_EXTERN setsockopt_O_REUSEADDR(Platform_Socket s, bool b);
    bool SOCKET_LITE_EXTERN setsockopt_O_KEEPALIVE(Platform_Socket s, bool b);
    bool SOCKET_LITE_EXTERN setsockopt_O_LINGER(Platform_Socket s, Linger_Option o);
    bool SOCKET_LITE_EXTERN setsockopt_O_OOBINLINE(Platform_Socket s, bool b);
    bool SOCKET_LITE_EXTERN setsockopt_O_EXCLUSIVEADDRUSE(Platform_Socket s, bool b);
    bool SOCKET_LITE_EXTERN setsockopt_O_SNDBUF(Platform_Socket s, int b);
    bool SOCKET_LITE_EXTERN setsockopt_O_RCVBUF(Platform_Socket s, int b);
    bool SOCKET_LITE_EXTERN setsockopt_O_SNDTIMEO(Platform_Socket s, std::chrono::seconds sec);
    bool SOCKET_LITE_EXTERN setsockopt_O_RCVTIMEO(Platform_Socket s, std::chrono::seconds sec);
    bool SOCKET_LITE_EXTERN setsockopt_O_NODELAY(Platform_Socket s, bool b);

    class SOCKET_LITE_EXTERN ISocket : public std::enable_shared_from_this<ISocket> {

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

        template <Socket_Options SO> struct setsockopt_factory_impl;
        template <> struct setsockopt_factory_impl<Socket_Options::O_DEBUG> {
            static auto setsockopt_(Platform_Socket handle, bool b) { return setsockopt_O_DEBUG(handle, b); }
        };
        template <> struct setsockopt_factory_impl<Socket_Options::O_BROADCAST> {
            static auto setsockopt_(Platform_Socket handle, bool b) { return setsockopt_O_BROADCAST(handle, b); }
        };
        template <> struct setsockopt_factory_impl<Socket_Options::O_REUSEADDR> {
            static auto setsockopt_(Platform_Socket handle, bool b) { return setsockopt_O_REUSEADDR(handle, b); }
        };
        template <> struct setsockopt_factory_impl<Socket_Options::O_KEEPALIVE> {
            static auto setsockopt_(Platform_Socket handle, bool b) { return setsockopt_O_KEEPALIVE(handle, b); }
        };
        template <> struct setsockopt_factory_impl<Socket_Options::O_LINGER> {
            static auto setsockopt_(Platform_Socket handle, Linger_Option b) { return setsockopt_O_LINGER(handle, b); }
        };
        template <> struct setsockopt_factory_impl<Socket_Options::O_OOBINLINE> {
            static auto setsockopt_(Platform_Socket handle, bool b) { return setsockopt_O_OOBINLINE(handle, b); }
        };
        template <> struct setsockopt_factory_impl<Socket_Options::O_EXCLUSIVEADDRUSE> {
            static auto setsockopt_(Platform_Socket handle, bool b) { return setsockopt_O_EXCLUSIVEADDRUSE(handle, b); }
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
            static auto setsockopt_(Platform_Socket handle, bool b) { return setsockopt_O_NODELAY(handle, b); }
        };

      public:
        Platform_Socket handle;
        ISocket();
        virtual ~ISocket();
        template <Socket_Options SO> auto getsockopt() const { return getsockopt_factory_impl<SO>::getsockopt_(this); }
        template <Socket_Options SO, typename... Args> auto setsockopt(Args &&... args)
        {
            return getsockopt_factory_impl<SO>::setsockopt_(this, std::forward<Args>(args)...);
        }
        std::optional<SocketInfo> get_PeerInfo() const { return SL::NET::get_PeerInfo(handle); }
        virtual void async_read(size_t buffer_size, unsigned char *buffer, const std::function<void(Bytes_Transfered)> &&handler) = 0;
        virtual void async_write(size_t buffer_size, unsigned char *buffer, const std::function<void(Bytes_Transfered)> &&handler) = 0;
        // send a close message and close the socket
        virtual void close() = 0;
    };

    class SOCKET_LITE_EXTERN IContext {
      public:
        virtual ~IContext() {}
    };
    class SOCKET_LITE_EXTERN IClientContext : public IContext {
      public:
        virtual ~IClientContext() {}
        std::function<void(const std::shared_ptr<ISocket> &)> onConnection;
        virtual bool async_connect(std::string host, PortNumber port) = 0;
        virtual void run(ThreadCount threadcount) = 0;
    };
    class SOCKET_LITE_EXTERN IListenContext : public IContext {
      public:
        virtual ~IListenContext() {}
        std::function<void(const std::shared_ptr<ISocket> &)> onConnection;
        virtual bool bind(PortNumber port) = 0;
        virtual bool listen() = 0;
        virtual void run(ThreadCount threadcount) = 0;
    };

    class SOCKET_LITE_EXTERN IListenerBind_Configuration {
      public:
        virtual ~IListenerBind_Configuration() {}
        virtual std::shared_ptr<IContext> listen() = 0;
    };
    class SOCKET_LITE_EXTERN IListener_Configuration {
      public:
        virtual ~IListener_Configuration() {}

        // when a connection is fully established.
        virtual std::shared_ptr<IListener_Configuration> onConnection(const std::function<void(const std::shared_ptr<ISocket> &)> &handle) = 0;
        // start the process to listen for clients. This is non-blocking and will return immediatly
        virtual std::shared_ptr<IListenerBind_Configuration> bind(PortNumber port) = 0;
    };

    class SOCKET_LITE_EXTERN IClient_Configuration {
      public:
        virtual ~IClient_Configuration() {}
        // when a connection is fully established.  If onconnect is called, then a matching onDisconnection is guaranteed
        virtual std::shared_ptr<IClient_Configuration> onConnection(const std::function<void(const std::shared_ptr<ISocket> &)> &handle) = 0;
        // connect to an endpoint. This is non-blocking and will return immediatly. If the library is unable to establish a connection,
        // ondisconnection will be called.
        virtual std::shared_ptr<IContext> async_connect(const std::string &host, PortNumber port) = 0;
    };
    // use these for convenience
    std::shared_ptr<IClient_Configuration> SOCKET_LITE_EXTERN CreateClient(ThreadCount threadcount);
    std::shared_ptr<IListener_Configuration> SOCKET_LITE_EXTERN CreateListener(ThreadCount threadcount);
    // use these for manual setup
    std::shared_ptr<IListenContext> SOCKET_LITE_EXTERN CreateListener();
    std::shared_ptr<IClientContext> SOCKET_LITE_EXTERN CreateClient();
} // namespace NET
} // namespace SL