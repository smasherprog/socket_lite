
#include "Socket_Lite.h"
#include "defs.h"
#include <assert.h>
#ifdef _WIN32
#include <WinSock2.h>
#include <Windows.h>
#include <Ws2tcpip.h>
#include <mswsock.h>
typedef int SOCKLEN_T;
#else
#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
typedef socklen_t SOCKLEN_T;
#endif

#include <string.h>
#include <string>

namespace SL {
namespace NET {
     

    const unsigned char *SocketAddr(const sockaddr &s) { return s.SocketImpl; }
    int SocketAddrLen(const sockaddr &s) { return s.SocketImplLen; }
    const std::string &Host(const sockaddr &s) { return s.Host; }
    unsigned short Port(const sockaddr &s) { return s.Port; }
    AddressFamily Family(const sockaddr &s) { return s.Family; }

    sockaddr::sockaddr(const sockaddr &addr) : SocketImplLen(addr.SocketImplLen), Host(addr.Host), Port(addr.Port), Family(addr.Family)
    {
        memcpy(SocketImpl, addr.SocketImpl, sizeof(SocketImpl));
    }
    sockaddr::sockaddr(unsigned char *buffer, int len, const char *host, unsigned short port, AddressFamily family)
    {
        assert(static_cast<size_t>(len) < sizeof(SocketImpl));
        memcpy(SocketImpl, buffer, len);
        SocketImplLen = len;
        Host = host;
        Port = port;
        Family = family;
    } 

    StatusCode getaddrinfo(const char *nodename, PortNumber pServiceName, AddressFamily family, const std::function<void(const sockaddr&)>& callback)
    {
        ::addrinfo hints = {0};
        ::addrinfo *result(nullptr);
        memset(&hints, 0, sizeof(addrinfo));
        hints.ai_family = AF_UNSPEC;     /* Return IPv4 and IPv6 choices */
        hints.ai_socktype = SOCK_STREAM; /* We want a TCP socket */
        hints.ai_protocol = IPPROTO_TCP;
        hints.ai_flags = AI_PASSIVE; /* All interfaces */
        auto portstr = std::to_string(pServiceName.value);
        auto s = ::getaddrinfo(nodename, portstr.c_str(), &hints, &result);
        if (s != 0) {
            return TranslateError();
        }
        for (auto ptr = result; ptr != NULL; ptr = ptr->ai_next) {

            char str[INET_ADDRSTRLEN];
            if (ptr->ai_family == AF_INET6 && family == AddressFamily::IPV6) {
                auto so = (::sockaddr_in6 *)ptr->ai_addr;
                inet_ntop(AF_INET6, &so->sin6_addr, str, INET_ADDRSTRLEN);
                SL::NET::sockaddr tmp((unsigned char *)so, sizeof(sockaddr_in6), str, so->sin6_port, AddressFamily::IPV6);
                callback(tmp);
            }
            else if (ptr->ai_family == AF_INET && family == AddressFamily::IPV4) {
                auto so = (::sockaddr_in *)ptr->ai_addr;
                inet_ntop(AF_INET, &so->sin_addr, str, INET_ADDRSTRLEN);
                SL::NET::sockaddr tmp((unsigned char *)so, sizeof(sockaddr_in), str, so->sin_port, AddressFamily::IPV4);
                callback(tmp);
            }
        }
        freeaddrinfo(result);
        return StatusCode::SC_SUCCESS;
    }

    Socket::Socket(Context &c) : context(c)
    { 
        WriteContext.Context_ = ReadContext.Context_ = &c;
    }
    Socket::Socket(Socket &&sock) : handle(sock.handle), context(sock.context) { sock.handle = INVALID_SOCKET; }
    Socket::~Socket() { 
#if !_WIN32
        SocketGetter sg1(*this);
        auto whandler(WriteContext.getCompletionHandler());
        if (whandler) {
            WriteContext.reset();
            sg1.getPendingIO() -= 1;
            whandler(StatusCode::SC_CLOSED, 0);
        }
        auto whandler1(ReadContext.getCompletionHandler());
        if (whandler1) {
            ReadContext.reset();
            sg1.getPendingIO() -= 1;
            whandler1(StatusCode::SC_CLOSED, 0);
        }
#endif
    
    }
    void recv(Socket &socket, size_t buffer_size, unsigned char *buffer, std::function<void(StatusCode, size_t)> &&handler)
    {
        if (!socket.isopen())
            return;
        SocketGetter sg(socket);
        auto context = sg.getReadContext();
        context->buffer = buffer;
        context->transfered_bytes = 0;
        context->bufferlen = buffer_size;
        context->Context_ = sg.getContext();
        context->Socket_ = sg.getSocket();
        context->setCompletionHandler(std::move(handler));
        context->IOOperation = INTERNAL::IO_OPERATION::IoRead;
#if !_WIN32
        sg.getPendingIO() += 1;
        continue_io(true, context, sg.getPendingIO(), stacklevel, sg.getIOCPHandle());
#else
        continue_io(true, context, sg.getPendingIO());
#endif
    }
    void send(Socket &socket, size_t buffer_size, unsigned char *buffer, std::function<void(StatusCode, size_t)> &&handler)
    {
        if (!socket.isopen())
            return;
        thread_local int stacklevel = 0;
        stacklevel += 1;
        SocketGetter sg(socket);
        auto context = sg.getWriteContext();
        context->buffer = buffer;
        context->transfered_bytes = 0;
        context->bufferlen = buffer_size;
        context->Context_ = sg.getContext();
        context->Socket_ = sg.getSocket();
        context->setCompletionHandler(std::move(handler));
        context->IOOperation = INTERNAL::IO_OPERATION::IoWrite;
#if !_WIN32
        sg.getPendingIO() += 1;
        continue_io(true, context, sg.getPendingIO(), stacklevel, sg.getIOCPHandle());
#else
        continue_io(true, context, sg.getPendingIO());
#endif
    }   

    StatusCode TranslateError(int *errcode)
    {
#if _WIN32
        auto originalerr = WSAGetLastError();
        auto errorcode = errcode != nullptr ? *errcode : originalerr;
        switch (errorcode) {
        case WSAECONNRESET:
            return StatusCode::SC_ECONNRESET;
        case WSAETIMEDOUT:
        case WSAECONNABORTED:
            return StatusCode::SC_ETIMEDOUT;
        case WSAEWOULDBLOCK:
            return StatusCode::SC_EWOULDBLOCK;
#else
        auto originalerror = errno;
        auto errorcode = errcode != nullptr ? *errcode : originalerror;
        switch (errorcode) {

#endif

        default:
            return StatusCode::SC_CLOSED;
        };
    } // namespace NET

} // namespace NET
} // namespace SL
