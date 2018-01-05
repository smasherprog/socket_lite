#pragma once
#include <chrono>
#include <functional>
#include <memory>

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
    typedef Explicit<unsigned short, INTERNAL::PorNumbertTag> PortNumber;
    typedef Explicit<unsigned short, INTERNAL::ThreadCountTag> ThreadCount;

    enum class SocketStatus : unsigned char { CONNECTED, CLOSED };
    enum class NetworkProtocol { IPV4, IPV6 };

    class SOCKET_LITE_EXTERN ISocket : public std::enable_shared_from_this<ISocket> {
      public:
        virtual ~ISocket() {}
        virtual SocketStatus get_status() const = 0;
        virtual std::string get_address() const = 0;
        virtual unsigned short get_port() const = 0;
        virtual NetworkProtocol get_protocol() const = 0;
        virtual bool is_loopback() const = 0;
        virtual void async_read(size_t buffer_size, unsigned char *buffer, const std::function<void(size_t)> &handler) = 0;
        virtual void async_write(size_t buffer_size, unsigned char *buffer, const std::function<void(size_t)> &handler) = 0;
        // send a close message and close the socket
        virtual void close() = 0;
    };

    class SOCKET_LITE_EXTERN IContext {
      public:
        virtual ~IContext() {}
    };
    class SOCKET_LITE_EXTERN IListener_Configuration {
      public:
        virtual ~IListener_Configuration() {}

        // when a connection is fully established.  If onconnect is called, then a matching onDisconnection is guaranteed
        virtual std::shared_ptr<IListener_Configuration> onConnection(const std::function<void(const std::shared_ptr<ISocket> &)> &handle) = 0;
        // when a socket is closed down for ANY reason. If onconnect is called, then a matching onDisconnection is guaranteed
        virtual std::shared_ptr<IListener_Configuration> onDisconnection(const std::function<void(const std::shared_ptr<ISocket> &)> &handle) = 0;
        // start the process to listen for clients. This is non-blocking and will return immediatly
        virtual std::shared_ptr<IContext> listen() = 0;
    };

    class SOCKET_LITE_EXTERN IClient_Configuration {
      public:
        virtual ~IClient_Configuration() {}
        // when a connection is fully established.  If onconnect is called, then a matching onDisconnection is guaranteed
        virtual std::shared_ptr<IClient_Configuration> onConnection(const std::function<void(const std::shared_ptr<ISocket> &)> &handle) = 0;
        // when a socket is closed down for ANY reason. If onconnect is called, then a matching onDisconnection is guaranteed
        virtual std::shared_ptr<IClient_Configuration> onDisconnection(const std::function<void(const std::shared_ptr<ISocket> &)> &handle) = 0;
        // connect to an endpoint. This is non-blocking and will return immediatly. If the library is unable to establish a connection,
        // ondisconnection will be called.
        virtual std::shared_ptr<IContext> connect(const std::string &host, PortNumber port) = 0;
    };

    class SOCKET_LITE_EXTERN IContext_Configuration {
      public:
        virtual ~IContext_Configuration() {}

        virtual std::shared_ptr<IListener_Configuration> CreateListener(PortNumber port, NetworkProtocol protocol = NetworkProtocol::IPV4) = 0;
        virtual std::shared_ptr<IClient_Configuration> CreateClient() = 0;
    };

    std::shared_ptr<IContext_Configuration> SOCKET_LITE_EXTERN CreateContext(ThreadCount threadcount);

} // namespace NET
} // namespace SL