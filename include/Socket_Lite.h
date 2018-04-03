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
#include <WinSock2.h>
#include <Windows.h>
#include <mswsock.h>
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
        SC_EAGAIN,
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
        SC_NOTSUPPORTED,
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
        sockaddr(unsigned char *buffer, int len, const char *host, unsigned short port, AddressFamily family);
        sockaddr(const sockaddr &addr);
        const unsigned char *get_SocketAddr() const;
        int get_SocketAddrLen() const;
        std::string get_Host() const;
        unsigned short get_Port() const;
        AddressFamily get_Family() const;
    };
    class SocketGetter;
    class SOCKET_LITE_EXTERN Context {
      protected:
        std::vector<std::thread> Threads;
        ThreadCount ThreadCount_;
#if WIN32
        WSADATA wsaData;
        LPFN_CONNECTEX ConnectEx_ = nullptr;
        LPFN_ACCEPTEX AcceptEx_ = nullptr;
        HANDLE IOCPHandle = NULL;
#else
        int EventWakeFd = -1;
        int IOCPHandle = -1;
#endif

        std::atomic<int> PendingIO;

      public:
        Context(ThreadCount threadcount);
        ~Context();
        Context() = delete;
        Context(Context &) = delete;
        Context(Context &&) = delete;
        Context &operator=(Context &) = delete;
        void run();

        friend class Listener;
        friend class SocketGetter;
    };
    class SOCKET_LITE_EXTERN Socket {
      protected:
        PlatformSocket handle;
        Context &context;

      public:
        Socket(Socket &&);
        Socket(const Socket &) = delete;
        Socket(Context &);
        ~Socket();
        Socket &operator=(const Socket &) = delete;
        bool isopen() const;
        void close();
        std::optional<SL::NET::sockaddr> getpeername();
        std::optional<SL::NET::sockaddr> getsockname();

        template <SocketOptions SO> friend auto getsockopt(Socket &socket);
        template <SocketOptions SO, typename... Args> friend auto setsockopt(Socket &socket, Args &&... args);
        friend class SocketGetter;
    };
    class SOCKET_LITE_EXTERN Listener {
      protected:
        Context &Context_;
        Socket ListenSocket;
        AddressFamily Family;

      public:
        Listener(Context &context, PortNumber port, AddressFamily family, StatusCode &ec);
        ~Listener();
        Listener() = delete;
        Listener(Listener &) = delete;
        Listener(Listener &&);
        Listener &operator=(Listener &) = delete;
        void accept(const std::function<void(StatusCode, Socket)> &&handler);
        void close() { ListenSocket.close(); }
    };

    namespace INTERNAL {
        std::tuple<StatusCode, std::optional<SockOptStatus>> SOCKET_LITE_EXTERN getsockopt_O_DEBUG(PlatformSocket handle);
        std::tuple<StatusCode, std::optional<SockOptStatus>> SOCKET_LITE_EXTERN getsockopt_O_ACCEPTCONN(PlatformSocket handle);
        std::tuple<StatusCode, std::optional<SockOptStatus>> SOCKET_LITE_EXTERN getsockopt_O_BROADCAST(PlatformSocket handle);
        std::tuple<StatusCode, std::optional<SockOptStatus>> SOCKET_LITE_EXTERN getsockopt_O_REUSEADDR(PlatformSocket handle);
        std::tuple<StatusCode, std::optional<SockOptStatus>> SOCKET_LITE_EXTERN getsockopt_O_KEEPALIVE(PlatformSocket handle);
        std::tuple<StatusCode, std::optional<LingerOption>> SOCKET_LITE_EXTERN getsockopt_O_LINGER(PlatformSocket handle);
        std::tuple<StatusCode, std::optional<SockOptStatus>> SOCKET_LITE_EXTERN getsockopt_O_OOBINLINE(PlatformSocket handle);
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
    template <SocketOptions SO> auto getsockopt(Socket &socket) { return INTERNAL::getsockopt_factory_impl<SO>::getsockopt_(socket.handle); }
    template <SocketOptions SO, typename... Args> auto setsockopt(Socket &socket, Args &&... args)
    {
        return INTERNAL::setsockopt_factory_impl<SO>::setsockopt_(socket.handle, std::forward<Args>(args)...);
    }
    std::tuple<StatusCode, std::vector<sockaddr>> SOCKET_LITE_EXTERN getaddrinfo(const char *nodename, PortNumber pServiceName, AddressFamily family);
    void SOCKET_LITE_EXTERN connect(Socket &socket, SL::NET::sockaddr &address, std::function<void(StatusCode)> &&);
    void SOCKET_LITE_EXTERN recv(Socket &socket, size_t buffer_size, unsigned char *buffer, std::function<void(StatusCode, size_t)> &&handler);
    void SOCKET_LITE_EXTERN send(Socket &socket, size_t buffer_size, unsigned char *buffer, std::function<void(StatusCode, size_t)> &&handler);

} // namespace NET
} // namespace SL
