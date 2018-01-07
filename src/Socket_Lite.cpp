#include "ClientContext.h"
#include "ListenContext.h"
#include "Socket_Lite.h"
#include <assert.h>
#ifdef WIN32
#include "internal/windows/Structures.h"
#else
#include <sys/socket.h>
#include <sys/types.h>
#define INVALID_SOCKET -1
#define closesocket close
#endif
#include <iostream>
#include <string>

namespace SL {
namespace NET {
    class Client_Configuration final : public IClient_Configuration {
      public:
        std::shared_ptr<ClientContext> ClientContext_;
        ThreadCount ThreadCount_;
        Client_Configuration(const std::shared_ptr<ClientContext> &context, ThreadCount threadcount)
            : ClientContext_(context), ThreadCount_(threadcount)
        {
        }
        virtual ~Client_Configuration() {}

        virtual std::shared_ptr<IClient_Configuration> onConnection(const std::function<void(const std::shared_ptr<ISocket> &)> &handle) override
        {
            assert(!ClientContext_->onConnection);
            ClientContext_->onConnection = handle;
            return std::make_shared<Client_Configuration>(ClientContext_, ThreadCount_);
        }
        virtual std::shared_ptr<IClient_Configuration> onDisconnection(const std::function<void(const std::shared_ptr<ISocket> &)> &handle) override
        {
            assert(!ClientContext_->onDisconnection);
            ClientContext_->onDisconnection = handle;
            return std::make_shared<Client_Configuration>(ClientContext_, ThreadCount_);
        }

        virtual std::shared_ptr<IContext> async_connect(const std::string &host, PortNumber port) override
        {
            if (!ClientContext_->async_connect(host, port)) {
                std::cerr << "Error Calling ClientContext::async_connect()" << std::endl;
            }
            else {
                ClientContext_->run(ThreadCount_);
            }
            return ClientContext_;
        }
    };
    class ListenerBind_Configuration final : public IListenerBind_Configuration {
      public:
        std::shared_ptr<ListenContext> ListenContext_;
        ThreadCount ThreadCount_;
        ListenerBind_Configuration(const std::shared_ptr<ListenContext> &context, ThreadCount threadcount)
            : ListenContext_(context), ThreadCount_(threadcount)
        {
        }
        virtual ~ListenerBind_Configuration() {}
        virtual std::shared_ptr<IContext> listen() override
        {
            if (!ListenContext_->listen()) {
                std::cerr << "Error Calling ListenContext::listen()" << std::endl;
            }
            else {
                ListenContext_->run(ThreadCount_);
            }
            return ListenContext_;
        }
    };

    class Listener_Configuration final : public IListener_Configuration {
      public:
        std::shared_ptr<ListenContext> ListenContext_;
        ThreadCount ThreadCount_;
        Listener_Configuration(const std::shared_ptr<ListenContext> &context, ThreadCount threadcount)
            : ListenContext_(context), ThreadCount_(threadcount)
        {
        }
        virtual ~Listener_Configuration() {}

        virtual std::shared_ptr<IListener_Configuration> onConnection(const std::function<void(const std::shared_ptr<ISocket> &)> &handle) override
        {
            assert(!ListenContext_->onConnection);
            ListenContext_->onConnection = handle;
            return std::make_shared<Listener_Configuration>(ListenContext_, ThreadCount_);
        }
        virtual std::shared_ptr<IListener_Configuration> onDisconnection(const std::function<void(const std::shared_ptr<ISocket> &)> &handle) override
        {
            assert(!ListenContext_->onDisconnection);
            ListenContext_->onDisconnection = handle;
            return std::make_shared<Listener_Configuration>(ListenContext_, ThreadCount_);
        }
        virtual std::shared_ptr<IListenerBind_Configuration> bind(PortNumber port) override
        {
            if (!ListenContext_->bind(port)) {
                std::cerr << "Error Calling ListenContext::bind()" << std::endl;
            }
            return std::make_shared<ListenerBind_Configuration>(ListenContext_, ThreadCount_);
        }
    };

    std::shared_ptr<IClient_Configuration> CreateClient(ThreadCount threadcount)
    {
        return std::make_shared<Client_Configuration>(std::make_shared<ClientContext>(), threadcount);
    }
    std::shared_ptr<IListener_Configuration> CreateListener(ThreadCount threadcount)
    {
        return std::make_shared<Listener_Configuration>(std::make_shared<ListenContext>(), threadcount);
    }
    std::shared_ptr<IListenContext> SOCKET_LITE_EXTERN CreateListener() { return std::make_shared<ListenContext>(); }
    std::shared_ptr<IClientContext> SOCKET_LITE_EXTERN CreateClient() { return std::make_shared<ClientContext>(); }

    ISocket::ISocket()
    {
#ifdef WIN32
        handle = INVALID_SOCKET;
#else
        handle = -1;
#endif
    }

    ISocket::~ISocket()
    {
#ifdef WIN32
        if (handle != INVALID_SOCKET) {
            closesocket(handle);
        }
#else
        if (handle != -1) {
            close(handle);
        }
#endif
    }

    std::optional<Platform_Socket> create_and_bind(PortNumber port)
    {

        struct addrinfo hints = {0};
        struct addrinfo *result(nullptr), *rp(nullptr);
        int s = 0;
        Platform_Socket sfd;

        memset(&hints, 0, sizeof(struct addrinfo));
        hints.ai_family = AF_UNSPEC;     /* Return IPv4 and IPv6 choices */
        hints.ai_socktype = SOCK_STREAM; /* We want a TCP socket */
        hints.ai_flags = AI_PASSIVE;     /* All interfaces */
        auto portstr = std::to_string(port.value);
        s = getaddrinfo(NULL, portstr.c_str(), &hints, &result);
        if (s != 0) {
            std::cerr << "getaddrinfo" << gai_strerror(s) << std::endl;
            return std::nullopt;
        }

        for (rp = result; rp != NULL; rp = rp->ai_next) {
#ifdef WIN32
            sfd = WSASocketW(rp->ai_family, rp->ai_socktype, rp->ai_protocol, NULL, 0, WSA_FLAG_OVERLAPPED);

#else
            sfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
#endif
            if (sfd == INVALID_SOCKET)
                continue;
            if (bind(sfd, rp->ai_addr, rp->ai_addrlen) == 0) {
                /* We managed to bind successfully! */
                break;
            }
            closesocket(sfd);
        }

        if (rp == NULL) {
            std::cerr << "Could not bind" << std::endl;
            return std::nullopt;
        }

        freeaddrinfo(result);
        return std::optional<Platform_Socket>{sfd};
    }
    std::optional<Platform_Socket> create_and_connect(std::string host, PortNumber port)
    {

        struct addrinfo hints = {0};
        struct addrinfo *result(nullptr), *rp(nullptr);
        int s = 0;
        Platform_Socket sfd;

        memset(&hints, 0, sizeof(struct addrinfo));
        hints.ai_family = AF_UNSPEC;     /* Return IPv4 and IPv6 choices */
        hints.ai_socktype = SOCK_STREAM; /* We want a TCP socket */
        hints.ai_flags = AI_PASSIVE;     /* All interfaces */
        auto portstr = std::to_string(port.value);
        s = getaddrinfo(NULL, portstr.c_str(), &hints, &result);
        if (s != 0) {
            std::cerr << "getaddrinfo" << gai_strerror(s) << std::endl;
            return std::nullopt;
        }

        for (rp = result; rp != NULL; rp = rp->ai_next) {
#ifdef WIN32
            sfd = WSASocketW(rp->ai_family, rp->ai_socktype, rp->ai_protocol, NULL, 0, WSA_FLAG_OVERLAPPED);

#else
            sfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);

#endif
            if (sfd == INVALID_SOCKET)
                continue;
            if (!make_socket_non_blocking(sfd)) {
                continue;
            }
            if (auto connectresult = connect(sfd, rp->ai_addr, rp->ai_addrlen); connectresult == 0 ||
#ifdef WIN32
                                                                                connectresult == SOCKET_ERROR && WSAGetLastError() == WSAEWOULDBLOCK
#else
                                                                                errno == EINPROGRESS

#endif

            ) {
                /* We managed to connect successfully! */
                break;
            }
            closesocket(sfd);
        }

        if (rp == NULL) {
            std::cerr << "Could not bind" << std::endl;
            return std::nullopt;
        }

        freeaddrinfo(result);
        return std::optional<Platform_Socket>{sfd};
    }

    bool make_socket_non_blocking(Platform_Socket socket)
    {
#ifdef WIN32
        u_long iMode = 1; // set socket for non blocking
        if (auto nRet = ioctlsocket(socket, FIONBIO, &iMode); nRet == NO_ERROR) {
            return false;
        }
#else
        int flags, s;

        flags = fcntl(socket, F_GETFL, 0);
        if (flags == -1) {
            return false;
        }
        flags |= O_NONBLOCK;
        s = fcntl(socket, F_SETFL, flags);
        if (s == -1) {
            return false;
        }
#endif
        return true;
    }

    std::optional<bool> getsockopt_O_DEBUG(Platform_Socket s)
    {
        int value = 0;
        int valuelen = sizeof(value);
        if (getsockopt(s, SOL_SOCKET, SO_DEBUG, (char *)&value, &valuelen) == 0) {
            return std::optional<bool>(value != 0);
        }
        return std::nullopt;
    }

    std::optional<bool> getsockopt_O_ACCEPTCONN(Platform_Socket s)
    {
        int value = 0;
        int valuelen = sizeof(value);
        if (getsockopt(s, SOL_SOCKET, SO_ACCEPTCONN, (char *)&value, &valuelen) == 0) {
            return std::optional<bool>(value != 0);
        }
        return std::nullopt;
    }

    std::optional<bool> getsockopt_O_BROADCAST(Platform_Socket s)
    {
        int value = 0;
        int valuelen = sizeof(value);
        if (getsockopt(s, SOL_SOCKET, SO_BROADCAST, (char *)&value, &valuelen) == 0) {
            return std::optional<bool>(value != 0);
        }
        return std::nullopt;
    }

    std::optional<bool> getsockopt_O_REUSEADDR(Platform_Socket s)
    {
        int value = 0;
        int valuelen = sizeof(value);
        if (getsockopt(s, SOL_SOCKET, SO_REUSEADDR, (char *)&value, &valuelen) == 0) {
            return std::optional<bool>(value != 0);
        }
        return std::nullopt;
    }

    std::optional<bool> getsockopt_O_KEEPALIVE(Platform_Socket s)
    {
        int value = 0;
        int valuelen = sizeof(value);
        if (getsockopt(s, SOL_SOCKET, SO_KEEPALIVE, (char *)&value, &valuelen) == 0) {
            return std::optional<bool>(value != 0);
        }
        return std::nullopt;
    }

    std::optional<Linger_Option> getsockopt_O_LINGER(Platform_Socket s)
    {
        struct linger value;
        int valuelen = sizeof(value);
        if (getsockopt(s, SOL_SOCKET, SO_LINGER, (char *)&value, &valuelen) == 0) {
            return std::optional<Linger_Option>(
                {value.l_onoff == 0 ? Linger_Options::LINGER_OFF : Linger_Options::LINGER_ON, std::chrono::seconds(value.l_linger)});
        }
        return std::nullopt;
    }

    std::optional<bool> getsockopt_O_OOBINLINE(Platform_Socket s)
    {
        int value = 0;
        int valuelen = sizeof(value);
        if (getsockopt(s, SOL_SOCKET, SO_OOBINLINE, (char *)&value, &valuelen) == 0) {
            return std::optional<bool>(value != 0);
        }
        return std::nullopt;
    }

    std::optional<bool> getsockopt_O_EXCLUSIVEADDRUSE(Platform_Socket s)
    {
        int value = 0;
        int valuelen = sizeof(value);
        if (getsockopt(s, SOL_SOCKET, SO_EXCLUSIVEADDRUSE, (char *)&value, &valuelen) == 0) {
            return std::optional<bool>(value != 0);
        }
        return std::nullopt;
    }

    std::optional<int> getsockopt_O_SNDBUF(Platform_Socket s)
    {
        int value = 0;
        int valuelen = sizeof(value);
        if (getsockopt(s, SOL_SOCKET, SO_SNDBUF, (char *)&value, &valuelen) == 0) {
            return std::optional<int>(value);
        }
        return std::nullopt;
    }

    std::optional<int> getsockopt_O_RCVBUF(Platform_Socket s)
    {
        int value = 0;
        int valuelen = sizeof(value);
        if (getsockopt(s, SOL_SOCKET, SO_RCVBUF, (char *)&value, &valuelen) == 0) {
            return std::optional<int>(value);
        }
        return std::nullopt;
    }

    std::optional<std::chrono::seconds> getsockopt_O_SNDTIMEO(Platform_Socket s)
    {
#ifdef WIN32
        DWORD value = 0;
        int valuelen = sizeof(value);
        if (getsockopt(s, SOL_SOCKET, SO_SNDTIMEO, (char *)&value, &valuelen) == 0) {
            return std::optional<std::chrono::seconds>(value * 1000); // convert from ms to seconds
        }
#else
        struct timeval value = {0};
        int valuelen = sizeof(value);
        if (getsockopt(s, SOL_SOCKET, SO_SNDTIMEO, (char *)&value, &valuelen) == 0) {
            return std::optional<std::chrono::seconds>(value.tv_sec); // convert from ms to seconds
        }
#endif
        return std::nullopt;
    }

    std::optional<std::chrono::seconds> getsockopt_O_RCVTIMEO(Platform_Socket s)
    {
#ifdef WIN32
        DWORD value = 0;
        int valuelen = sizeof(value);
        if (getsockopt(s, SOL_SOCKET, SO_RCVTIMEO, (char *)&value, &valuelen) == 0) {
            return std::optional<std::chrono::seconds>(value * 1000); // convert from ms to seconds
        }
#else
        struct timeval value = {0};
        int valuelen = sizeof(value);
        if (getsockopt(s, SOL_SOCKET, SO_RCVTIMEO, (char *)&value, &valuelen) == 0) {
            return std::optional<std::chrono::seconds>(value.tv_sec); // convert from ms to seconds
        }
#endif
        return std::nullopt;
    }

    std::optional<int> getsockopt_O_ERROR(Platform_Socket s)
    {
        int value = 0;
        int valuelen = sizeof(value);
        if (getsockopt(s, SOL_SOCKET, SO_ERROR, (char *)&value, &valuelen) == 0) {
            return std::optional<int>(value);
        }
        return std::nullopt;
    }

    std::optional<bool> getsockopt_O_NODELAY(Platform_Socket s)
    {
        int value = 0;
        int valuelen = sizeof(value);
        if (getsockopt(s, IPPROTO_TCP, TCP_NODELAY, (char *)&value, &valuelen) == 0) {
            return std::optional<bool>(value != 0);
        }
        return std::nullopt;
    }

    bool setsockopt_O_DEBUG(Platform_Socket s, bool b)
    {
        int value = b ? 1 : 0;
        int valuelen = sizeof(value);
        return setsockopt(s, SOL_SOCKET, SO_DEBUG, (char *)&value, valuelen) == 0;
    }

    bool setsockopt_O_BROADCAST(Platform_Socket s, bool b)
    {
        int value = b ? 1 : 0;
        int valuelen = sizeof(value);
        return setsockopt(s, SOL_SOCKET, SO_BROADCAST, (char *)&value, valuelen) == 0;
    }

    bool setsockopt_O_REUSEADDR(Platform_Socket s, bool b)
    {
        int value = b ? 1 : 0;
        int valuelen = sizeof(value);
        return setsockopt(s, SOL_SOCKET, SO_BROADCAST, (char *)&value, valuelen) == 0;
    }

    bool setsockopt_O_KEEPALIVE(Platform_Socket s, bool b)
    {
        int value = b ? 1 : 0;
        int valuelen = sizeof(value);
        return setsockopt(s, SOL_SOCKET, SO_KEEPALIVE, (char *)&value, valuelen) == 0;
    }

    bool setsockopt_O_LINGER(Platform_Socket s, Linger_Option o)
    {
        struct linger value;
        value.l_onoff = o.l_onoff == Linger_Options::LINGER_OFF ? 0 : 1;
        value.l_linger = static_cast<unsigned short>(o.l_linger.count());
        int valuelen = sizeof(value);
        return setsockopt(s, SOL_SOCKET, SO_LINGER, (char *)&value, valuelen) == 0;
    }

    bool setsockopt_O_OOBINLINE(Platform_Socket s, bool b)
    {
        int value = b ? 1 : 0;
        int valuelen = sizeof(value);
        return setsockopt(s, SOL_SOCKET, SO_OOBINLINE, (char *)&value, valuelen) == 0;
    }

    bool setsockopt_O_EXCLUSIVEADDRUSE(Platform_Socket s, bool b)
    {
        int value = b ? 1 : 0;
        int valuelen = sizeof(value);
        return setsockopt(s, SOL_SOCKET, SO_EXCLUSIVEADDRUSE, (char *)&value, valuelen) == 0;
    }

    bool setsockopt_O_SNDBUF(Platform_Socket s, int b)
    {
        int value = b;
        int valuelen = sizeof(value);
        return setsockopt(s, SOL_SOCKET, SO_SNDBUF, (char *)&value, valuelen) == 0;
    }

    bool setsockopt_O_RCVBUF(Platform_Socket s, int b)
    {
        int value = b;
        int valuelen = sizeof(value);
        return setsockopt(s, SOL_SOCKET, SO_RCVBUF, (char *)&value, valuelen) == 0;
    }

    bool setsockopt_O_SNDTIMEO(Platform_Socket s, std::chrono::seconds sec)
    {
#ifdef WIN32
        DWORD value = static_cast<DWORD>(sec.count() * 1000); // convert to miliseconds for windows
        int valuelen = sizeof(value);
        return setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, (char *)&value, valuelen) == 0;
#else
        struct timeval tv = {0};
        tv.tv_sec = static_cast<long>(sec.count());
        return setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, (struct timeval *)&tv, sizeof(tv)) == 0;
#endif
    }

    bool setsockopt_O_RCVTIMEO(Platform_Socket s, std::chrono::seconds sec)
    {
#ifdef WIN32
        DWORD value = static_cast<DWORD>(sec.count() * 1000); // convert to miliseconds for windows
        int valuelen = sizeof(value);
        return setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, (char *)&value, valuelen) == 0;
#else
        struct timeval tv = {0};
        tv.tv_sec = static_cast<long>(sec.count());
        return setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, (struct timeval *)&tv, sizeof(tv)) == 0;
#endif
    }

    bool setsockopt_O_NODELAY(Platform_Socket s, bool b)
    {
        int value = b ? 1 : 0;
        int valuelen = sizeof(value);
        return setsockopt(s, IPPROTO_TCP, TCP_NODELAY, (char *)&value, valuelen) == 0;
    }

} // namespace NET
} // namespace SL