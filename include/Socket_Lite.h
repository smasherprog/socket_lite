#pragma once

#include <chrono>
#include <functional>
#include <memory>
#if __APPLE__
#include <experimental/optional>
#else
#include <optional>
#endif
#include <stdint.h>
#include <string>
#include <thread>
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

    class SOCKET_LITE_EXTERN SocketAddress {
        unsigned char SocketImpl[65] = {0};
        int SocketImplLen = 0;
        std::string Host;
        unsigned short Port = 0;
        AddressFamily Family = AddressFamily::IPV4;

      public:
        SocketAddress() {}
        SocketAddress(unsigned char *buffer, int len, const char *host, unsigned short port, AddressFamily family);
        SocketAddress(const SocketAddress &addr);

        const unsigned char *getSocketAddr() const;
        int getSocketAddrLen() const;
        const std::string &getHost() const;
        unsigned short getPort() const;
        AddressFamily getFamily() const;
    };
    const unsigned char *SOCKET_LITE_EXTERN SocketAddr(const SocketAddress &);
    int SOCKET_LITE_EXTERN SocketAddrLen(const SocketAddress &);
    const std::string &SOCKET_LITE_EXTERN Host(const SocketAddress &);
    unsigned short SOCKET_LITE_EXTERN Port(const SocketAddress &);
    AddressFamily SOCKET_LITE_EXTERN Family(const SocketAddress &);
#ifdef _WIN32
#if ((UINTPTR_MAX) == (UINT_MAX))
    typedef Explicit<unsigned short, SocketHandleTag> SocketHandle;
#else
    typedef Explicit<unsigned long long, SocketHandleTag> SocketHandle;
#endif
#else
    typedef Explicit<int, SocketHandleTag> SocketHandle;
#endif

    class SOCKET_LITE_EXTERN PlatformSocket {
        SocketHandle Handle_;

      public:
        PlatformSocket();
        PlatformSocket(SocketHandle h);
        PlatformSocket(const AddressFamily &family, Blocking_Options opts);
        ~PlatformSocket();
        PlatformSocket(const PlatformSocket &) = delete;
        PlatformSocket(PlatformSocket &&);
        PlatformSocket &operator=(PlatformSocket &) = delete;
        PlatformSocket &operator=(PlatformSocket &&);
        operator bool() const;
        void close();
        void shutdown(ShutDownOptions o);
        [[nodiscard]] SocketHandle Handle() const { return Handle_; };

        StatusCode getsockopt(DEBUGTag, const std::function<void(const SockOptStatus &)> &callback) const;
        StatusCode getsockopt(ACCEPTCONNTag, const std::function<void(const SockOptStatus &)> &callback) const;
        StatusCode getsockopt(BROADCASTTag, const std::function<void(const SockOptStatus &)> &callback) const;
        StatusCode getsockopt(REUSEADDRTag, const std::function<void(const SockOptStatus &)> &callback) const;
        StatusCode getsockopt(KEEPALIVETag, const std::function<void(const SockOptStatus &)> &callback) const;
        StatusCode getsockopt(LINGERTag, const std::function<void(const LingerOption &)> &callback) const;
        StatusCode getsockopt(OOBINLINETag, const std::function<void(const SockOptStatus &)> &callback) const;
        StatusCode getsockopt(SNDBUFTag, const std::function<void(const int &)> &callback) const;
        StatusCode getsockopt(RCVBUFTag, const std::function<void(const int &)> &callback) const;
        StatusCode getsockopt(SNDTIMEOTag, const std::function<void(const std::chrono::seconds &)> &callback) const;
        StatusCode getsockopt(RCVTIMEOTag, const std::function<void(const std::chrono::seconds &)> &callback) const;
        StatusCode getsockopt(ERRORTag, const std::function<void(const int &)> &callback) const;
        StatusCode getsockopt(NODELAYTag, const std::function<void(const SockOptStatus &)> &callback) const;
        StatusCode getsockopt(BLOCKINGTag, const std::function<void(const Blocking_Options &)> &callback) const;

        StatusCode getpeername(const std::function<void(const SocketAddress &)> &callback) const;
        StatusCode getsockname(const std::function<void(const SocketAddress &)> &callback) const;

        StatusCode setsockopt(DEBUGTag, SockOptStatus b);
        StatusCode setsockopt(BROADCASTTag, SockOptStatus b);
        StatusCode setsockopt(REUSEADDRTag, SockOptStatus b);
        StatusCode setsockopt(KEEPALIVETag, SockOptStatus b);
        StatusCode setsockopt(LINGERTag, LingerOption o);
        StatusCode setsockopt(OOBINLINETag, SockOptStatus b);
        StatusCode setsockopt(SNDBUFTag, int b);
        StatusCode setsockopt(RCVBUFTag, int b);
        StatusCode setsockopt(SNDTIMEOTag, std::chrono::seconds sec);
        StatusCode setsockopt(RCVTIMEOTag, std::chrono::seconds sec);
        StatusCode setsockopt(NODELAYTag, SockOptStatus b);
        StatusCode setsockopt(BLOCKINGTag, Blocking_Options b);

        StatusCode listen(int backlog);
        StatusCode bind(const SocketAddress &addr);
        std::tuple<StatusCode, size_t> send(unsigned char *buf, size_t len, int flags);
        std::tuple<StatusCode, size_t> recv(unsigned char *buf, size_t len, int flags);
    };
    // forward declares
    class Context;
    class SOCKET_LITE_EXTERN Socket {
      protected:
        PlatformSocket PlatformSocket_;
        Context &IOData_;
        void recv_success(size_t, unsigned char *, std::function<void(StatusCode, size_t)> &&, size_t);
        void recv_continue(size_t, unsigned char *, std::function<void(StatusCode, size_t)> &&);
        void send_success(size_t, unsigned char *, std::function<void(StatusCode, size_t)> &&, size_t);
        void send_continue(size_t, unsigned char *, std::function<void(StatusCode, size_t)> &&);

      public:
        Socket(Context &, PlatformSocket &&);
        Socket(Socket &&);
        Socket(Context &);
        ~Socket();
        [[nodiscard]] PlatformSocket &Handle() { return PlatformSocket_; }
        void close();
        template <class FUNC> void recv_async(size_t buffer_size, unsigned char *buffer, FUNC &&handler) { 
            auto[code, bytes] = PlatformSocket_.recv(buffer, buffer_size, 0);
            if (code == StatusCode::SC_SUCCESS) {
                static int counter = 0;
                if (counter++ % 4 != 0 && bytes == buffer_size) {
                    // execute callback meow!
                    return handler(StatusCode::SC_SUCCESS, bytes);
                }
                else {
                    return recv_success(buffer_size, buffer, std::move(handler), bytes);
                }
            }
            else if (code == StatusCode::SC_CLOSED) {
                return handler(code, 0);
            }
            recv_continue(buffer_size, buffer, std::move(handler));
        }
        template <class FUNC> void send_async(size_t buffer_size, unsigned char *buffer, FUNC &&handler)
        {  
             auto[code, bytes] = PlatformSocket_.send(buffer, buffer_size, 0);
            if (code == StatusCode::SC_SUCCESS) {
                static int counter = 0;
                if (counter++ % 4 != 0 && bytes == buffer_size) {
                    // execute callback meow!
                    return handler(StatusCode::SC_SUCCESS, bytes);
                }
                else {
                    return send_success(buffer_size, buffer, std::move(handler), bytes);
                }
            }
            else if (code == StatusCode::SC_CLOSED) {
                return handler(code, 0);
            }
            send_continue(buffer_size, buffer, std::move(handler));
        }
        friend class Context;
        friend void connect_async(Socket &socket, SocketAddress &address, std::function<void(StatusCode)> &&handler);
    };

    [[nodiscard]] std::vector<SL::NET::SocketAddress> SOCKET_LITE_EXTERN getaddrinfo(const char *nodename, PortNumber port,
                                                                                     AddressFamily family = AddressFamily::IPANY);

    void SOCKET_LITE_EXTERN connect_async(Socket &socket, SL::NET::SocketAddress &address, std::function<void(StatusCode)> &&);

    class SOCKET_LITE_EXTERN INetworkConfig {
      public:
        virtual std::shared_ptr<INetworkConfig> AddListener(PlatformSocket &&, std::function<void(Socket)> &&) = 0;
        virtual std::shared_ptr<Context> run() = 0;
    };
    std::shared_ptr<INetworkConfig> SOCKET_LITE_EXTERN createContext(ThreadCount threadcount = ThreadCount(std::thread::hardware_concurrency()));

} // namespace NET
} // namespace SL
