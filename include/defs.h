#pragma once

#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
#include <optional>
#include <stdint.h>
#include <tuple>
#include <variant>
#include <vector>

#if defined(WINDOWS) || defined(_WIN32)
#include <WinSock2.h>
#include <Windows.h>
#include <Ws2tcpip.h>
#include <mswsock.h>
typedef int SOCKLEN_T;
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

#ifdef _WIN32
    struct WSARAII {
        WSADATA wsaData;
        WSARAII()
        {
            if (auto ret = WSAStartup(0x202, &wsaData); ret != 0) {
                //  std::cerr << "WSAStartup() failed " << GetLastError() << std::endl;
            }
        }
        ~WSARAII()
        {
            if (auto ret = WSACleanup(); ret != 0) {
            }
        }
    };
#endif

    struct IOCP {
#ifdef _WIN32
        HANDLE handle = NULL;
#else
        int handle = -1;
#endif
        IOCP(int numberOfConcurrentThreads);
        ~IOCP();
        operator bool() const;
    };

    StatusCode SOCKET_LITE_EXTERN TranslateError(int *errcode = nullptr);
    enum IO_OPERATION { IoNone, IoInitConnect, IoConnect, IoStartAccept, IoAccept, IoRead, IoWrite };
    class Socket;
    struct Win_IO_Context {
#ifdef _WIN32
        WSAOVERLAPPED Overlapped = {0};
#endif
        IO_OPERATION IOOperation = IO_OPERATION::IoNone;
    };

    struct Win_IO_Connect_Context : Win_IO_Context {
        SL::NET::sockaddr address;
        std::function<void(StatusCode)> completionhandler;
        Socket *Socket_ = nullptr;
        void clear()
        {
            completionhandler = nullptr;
            Socket_ = nullptr;
        }
    };
    struct Win_IO_Accept_Context : Win_IO_Context {
        std::shared_ptr<Socket> Socket_;
        SOCKET ListenSocket = INVALID_SOCKET;
        std::function<void(StatusCode, const std::shared_ptr<Socket> &)> completionhandler;
    };
    struct RW_CompletionHandler {
        RW_CompletionHandler()
        {
            Completed = 1;
            RefCount = 0;
        }
        std::function<void(StatusCode, size_t)> completionhandler;
        std::atomic<int> Completed;
        std::atomic<int> RefCount;
        void handle(StatusCode code, size_t bytes, bool lockneeded)
        {
            if (lockneeded) {
                if (Completed.fetch_sub(1, std::memory_order_relaxed) == 1) {
                    completionhandler(code, bytes);
                }
            }
            else {
                completionhandler(code, bytes);
            }
        }
        void clear()
        {
            RefCount = 0;
            Completed = 1;
            completionhandler = nullptr;
        }
    };
    struct Win_IO_RW_Context : Win_IO_Context {
        size_t transfered_bytes = 0;
        size_t bufferlen = 0;
        unsigned char *buffer = nullptr;
        RW_CompletionHandler *completionhandler = nullptr;
        void clear()
        {
            transfered_bytes = 0;
            bufferlen = 0;
            buffer = nullptr;
            completionhandler = nullptr;
#ifdef _WIN32
            Overlapped = {0};
#endif
            IOOperation = IO_OPERATION::IoNone;
        }
    };
    struct Win_IO_RW_ContextCRUD {
        Win_IO_RW_Context *newObj() { return new Win_IO_RW_Context(); }
        void deleteObj(Win_IO_RW_Context *p) { delete p; }
        void clearObj(Win_IO_RW_Context *p) { p->clear(); }
    };
    struct RW_CompletionHandlerCRUD {
        RW_CompletionHandler *newObj() { return new RW_CompletionHandler(); }
        void deleteObj(RW_CompletionHandler *p) { delete p; }
        void clearObj(RW_CompletionHandler *p) { p->clear(); }
    };

} // namespace NET
} // namespace SL
