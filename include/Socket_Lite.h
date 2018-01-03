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

    enum class SocketStatus : unsigned char { CONNECTING, CONNECTED, CLOSING, CLOSED };
    enum class NetworkProtocol { IPV4, IPV6 };
    struct Message {
        unsigned char *data;
        size_t len;
        // buffer is here to ensure the lifetime of the unsigned char *data in this structure
        // users should set the *data variable to be the beginning of the data to send. Then, set the Buffer shared ptr as well to make sure the
        // lifetime of the data
        std::shared_ptr<unsigned char> Buffer;
    };
    class SOCKET_LITE_EXTERN ISocket {
      public:
        virtual ~ISocket() {}
        virtual SocketStatus is_open() const = 0;
        virtual std::string get_address() const = 0;
        virtual unsigned short get_port() const = 0;
        virtual NetworkProtocol get_protocol() const = 0;
        virtual bool is_loopback() const = 0;
        virtual size_t BufferedBytes() const = 0;
        virtual void send(const Message &msg) = 0;
        // send a close message and close the socket
        virtual void close() = 0;
    };
    class SOCKET_LITE_EXTERN IContext {
      public:
        virtual ~IContext() {}
        // the maximum payload size
        virtual void set_MaxPayload(size_t bytes) = 0;
        // the maximum payload size
        virtual size_t get_MaxPayload() = 0;
        // maximum time in seconds before a client is considered disconnected -- for reads
        virtual void set_ReadTimeout(std::chrono::seconds seconds) = 0;
        // get the current read timeout in seconds
        virtual std::chrono::seconds get_ReadTimeout() = 0;
        // maximum time in seconds before a client is considered disconnected -- for writes
        virtual void set_WriteTimeout(std::chrono::seconds seconds) = 0;
        // get the current write timeout in seconds
        virtual std::chrono::seconds get_WriteTimeout() = 0;
    };
    class SOCKET_LITE_EXTERN IListener_Configuration {
      public:
        virtual ~IListener_Configuration() {}

        // when a connection is fully established.  If onconnect is called, then a matching onDisconnection is guaranteed
        virtual std::shared_ptr<IListener_Configuration> onConnection(const std::function<void(const std::shared_ptr<ISocket> &)> &handle) = 0;
        // when a message has been received
        virtual std::shared_ptr<IListener_Configuration>
        onMessage(const std::function<void(const std::shared_ptr<ISocket> &, const Message &)> &handle) = 0;
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
        // when a message has been received
        virtual std::shared_ptr<IClient_Configuration>
        onMessage(const std::function<void(const std::shared_ptr<ISocket> &, const Message &)> &handle) = 0;
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