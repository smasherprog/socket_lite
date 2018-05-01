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
        struct SocketHandleTag {};
        struct PorNumbertTag {};
        struct ThreadCountTag {};
        template <typename T, typename Meaning> struct Explicit {
            Explicit(T value) : value(value) {}
            T value;
        };
        struct DEBUGTag {};
        struct ACCEPTCONNTag {};
        struct BROADCASTTag {};
        struct REUSEADDRTag {};
        struct KEEPALIVETag {};
        struct LINGERTag {};
        struct OOBINLINETag {};
        struct SNDBUFTag {};
        struct RCVBUFTag {};
        struct SNDTIMEOTag {};
        struct RCVTIMEOTag {};
        struct ERRORTag {};
        struct NODELAYTag {};
        struct BLOCKINGTag {};

        enum class Blocking_Options { BLOCKING, NON_BLOCKING };
        typedef Explicit<unsigned short, ThreadCountTag> ThreadCount;
        typedef Explicit<unsigned short, PorNumbertTag> PortNumber;

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
        struct SOCKET_LITE_EXTERN sockaddr;
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
            PlatformSocket(const AddressFamily& family);
            ~PlatformSocket();
            PlatformSocket(PlatformSocket &) = delete;
            PlatformSocket(PlatformSocket &&);
            PlatformSocket &operator=(PlatformSocket &) = delete;
            operator bool() const;
            StatusCode close();
            SocketHandle Handle() const { return Handle_; };

            StatusCode getsockopt(DEBUGTag, const std::function<void(const SockOptStatus&)>& callback) const;
            StatusCode getsockopt(ACCEPTCONNTag, const std::function<void(const SockOptStatus&)>& callback)const;
            StatusCode getsockopt(BROADCASTTag, const std::function<void(const SockOptStatus&)>& callback)const;
            StatusCode getsockopt(REUSEADDRTag, const std::function<void(const SockOptStatus&)>& callback)const;
            StatusCode getsockopt(KEEPALIVETag, const std::function<void(const SockOptStatus&)>& callback)const;
            StatusCode getsockopt(LINGERTag, const std::function<void(const LingerOption&)>& callback)const;
            StatusCode getsockopt(OOBINLINETag, const std::function<void(const SockOptStatus&)>& callback)const;
            StatusCode getsockopt(SNDBUFTag, const std::function<void(const int&)>& callback)const;
            StatusCode getsockopt(RCVBUFTag, const std::function<void(const int&)>& callback)const;
            StatusCode getsockopt(SNDTIMEOTag, const std::function<void(const std::chrono::seconds&)>& callback)const;
            StatusCode getsockopt(RCVTIMEOTag, const std::function<void(const std::chrono::seconds&)>& callback)const;
            StatusCode getsockopt(ERRORTag, const std::function<void(const int&)>& callback)const;
            StatusCode getsockopt(NODELAYTag, const std::function<void(const SockOptStatus&)>& callback)const;
            StatusCode getsockopt(BLOCKINGTag, const std::function<void(const Blocking_Options&)>& callback)const;

            StatusCode getpeername(const std::function<void(const sockaddr&)>& callback)const;
            StatusCode getsockname(const std::function<void(const sockaddr&)>& callback)const;

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
            StatusCode bind(const sockaddr& addr);
        };
    }
}
#include "Socket_Liteimpl.h"
namespace SL {
    namespace NET {
        class SOCKET_LITE_EXTERN Context {
        protected:
            INTERNAL::ContextImpl ContextImpl_;
        public:
            Context(ThreadCount threadcount);
            ~Context();
            Context() = delete;
            Context(Context &) = delete;
            Context(Context &&) = delete;
            Context &operator=(Context &) = delete;
            void run();

            friend class Listener; 
        };

        class SOCKET_LITE_EXTERN Socket {
        protected:
            PlatformSocket handle;
            Context &context;
            INTERNAL::Win_IO_RW_Context ReadContext;
            INTERNAL::Win_IO_RW_Context WriteContext;

        public:
            Socket(Context &, PlatformSocket&&);
            Socket(Socket &&);
            Socket(const Socket &) = delete;
            Socket(Context &);
            ~Socket();
            Socket &operator=(const Socket &) = delete;
            PlatformSocket& Handle() { return handle; }
            const PlatformSocket& Handle() const { return handle; }


        };
        struct Acceptor {
            PlatformSocket AcceptSocket;
            AddressFamily Family;
            std::function<void(Socket)> AcceptHandler;
        };

        class SOCKET_LITE_EXTERN Listener {
        protected:
            Context & Context_;
            Acceptor Acceptor_;
            std::thread Runner;
            bool Keepgoing;

        public:
            Listener(Context &c, Acceptor &&Acceptor);
            ~Listener();
            Listener() = delete;
            Listener(Listener &) = delete;
            Listener(Listener &&) = delete;
            Listener &operator=(Listener &) = delete;
            void close();
            void start();
            void stop();
            bool isStopped() const;
        };
        enum CallbackStatus { CONTINUE, FINISHED };
        //this is a synch call and will call the callback for each address found 
        StatusCode SOCKET_LITE_EXTERN getaddrinfo(const char *nodename, PortNumber port, AddressFamily family, const std::function<CallbackStatus(const sockaddr&)>& callback);

        void SOCKET_LITE_EXTERN connect(Socket &socket, SL::NET::sockaddr &address, std::function<void(StatusCode)> &&);
        void SOCKET_LITE_EXTERN recv(Socket &socket, size_t buffer_size, unsigned char *buffer, std::function<void(StatusCode, size_t)> &&handler);
        void SOCKET_LITE_EXTERN send(Socket &socket, size_t buffer_size, unsigned char *buffer, std::function<void(StatusCode, size_t)> &&handler);

    } // namespace NET
} // namespace SL
