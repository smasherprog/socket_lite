#pragma once

#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
#include <optional>
#include <stdint.h>
#include <thread>
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
    enum class AddressFamily { IPV4, IPV6 };

    struct LingerOption {
        LingerOptions l_onoff;         /* option on/off */
        std::chrono::seconds l_linger; /* linger time */
    };

    class SOCKET_LITE_EXTERN sockaddr {
        unsigned char SocketImpl[65] = {0};
        int SocketImplLen = 0;
        std::string Host;
        unsigned short Port = 0;
        AddressFamily Family = AddressFamily::IPV4;

      public:
        sockaddr() {}
        sockaddr(unsigned char *buffer, int len, const char *host, unsigned short port, AddressFamily family);
        sockaddr(const sockaddr &addr);

        const unsigned char *getSocketAddr() const;
        int getSocketAddrLen() const;
        const std::string &getHost() const;
        unsigned short getPort() const;
        AddressFamily getFamily() const;
    };
    const unsigned char *SOCKET_LITE_EXTERN SocketAddr(const sockaddr &);
    int SOCKET_LITE_EXTERN SocketAddrLen(const sockaddr &);
    const std::string &SOCKET_LITE_EXTERN Host(const sockaddr &);
    unsigned short SOCKET_LITE_EXTERN Port(const sockaddr &);
    AddressFamily SOCKET_LITE_EXTERN Family(const sockaddr &);
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
        PlatformSocket(const AddressFamily &family);
        ~PlatformSocket();
        PlatformSocket(const PlatformSocket &) = delete;
        PlatformSocket(PlatformSocket &&);
        PlatformSocket &operator=(PlatformSocket &) = delete;
        PlatformSocket &operator=(PlatformSocket &&);
        operator bool() const;
        void close();
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

        StatusCode getpeername(const std::function<void(const sockaddr &)> &callback) const;
        StatusCode getsockname(const std::function<void(const sockaddr &)> &callback) const;

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
        StatusCode bind(const sockaddr &addr);
    };
    // forward declares
    class ContextImpl;
    class Context;
    class Win_IO_Context;

    class SOCKET_LITE_EXTERN Socket {
      protected:
        PlatformSocket PlatformSocket_;
        ContextImpl &Context_;
        Win_IO_Context *ReadContext_, *WriteContext;

      public:
        Socket(Context &, PlatformSocket &&);
        Socket(ContextImpl &, PlatformSocket &&);
        Socket(Socket &&);
        Socket(Context &);
        Socket(ContextImpl &);
        ~Socket();
        Socket(const Socket &) = delete;
        Socket &operator=(const Socket &) = delete;
        [[nodiscard]] PlatformSocket &Handle() { return PlatformSocket_; }
        const PlatformSocket &Handle() const { return PlatformSocket_; }
        void close();

        // guarantees async behavior and will not complete immediatly, but some time later
        void recv_async(size_t buffer_size, unsigned char *buffer, std::function<void(StatusCode, size_t)> &&handler);
        void send_async(size_t buffer_size, unsigned char *buffer, std::function<void(StatusCode, size_t)> &&handler);
        friend void connect_async(Socket &context, SL::NET::sockaddr &address, std::function<void(StatusCode)> &&);
    };
    class SOCKET_LITE_EXTERN Context {
      protected:
        ContextImpl *ContextImpl_;

      public:
        Context(ThreadCount threadcount = ThreadCount(std::thread::hardware_concurrency()));
        ~Context();
        Context() = delete;
        Context(const Context &) = delete;
        Context(Context &&) = delete;
        Context &operator=(Context &) = delete;
        void run();

        friend class Listener;
        friend class Socket;
    };
    struct Acceptor {
        PlatformSocket AcceptSocket;
        AddressFamily Family;
        std::function<void(Socket)> AcceptHandler;
    };

    class SOCKET_LITE_EXTERN Listener {
      protected:
        ContextImpl &ContextImpl_;
        Acceptor Acceptor_;
        std::thread Runner;
        bool Keepgoing;

      public:
        Listener(Context &c, Acceptor &&Acceptor);
        ~Listener();
        Listener() = delete;
        Listener(const Listener &) = delete;
        Listener(Listener &&) = delete;
        Listener &operator=(Listener &) = delete;
        void start();
        // this will block
        void stop();
        [[nodiscard]] bool isStopped() const;
    };
    enum GetAddrInfoCBStatus { CONTINUE, FINISHED };
    // this is a sync call and will call the callback for each address found
    [[nodiscard]] StatusCode SOCKET_LITE_EXTERN getaddrinfo(const char *nodename, PortNumber port, AddressFamily family,
                                                            const std::function<GetAddrInfoCBStatus(const sockaddr &)> &callback);

    void SOCKET_LITE_EXTERN connect_async(Socket &socket, SL::NET::sockaddr &address, std::function<void(StatusCode)> &&);

} // namespace NET
} // namespace SL
