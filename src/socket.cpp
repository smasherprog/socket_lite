#include "socket.h"
#include "io_service.h"

#if _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <MSWSock.h>
#include <WS2tcpip.h>
#include <WinSock2.h>
#include <Windows.h>
#endif

namespace SL::Network {

namespace local {
    ip_endpoint sockaddr_to_ip_endpoint(const sockaddr &address) noexcept
    {
        if (address.sa_family == AF_INET) {
            SOCKADDR_IN ipv4Address;
            std::memcpy(&ipv4Address, &address, sizeof(ipv4Address));
            std::uint8_t addressBytes[4];
            std::memcpy(addressBytes, &ipv4Address.sin_addr, 4);
            return ipv4_endpoint{ipv4_address{addressBytes}, ntohs(ipv4Address.sin_port)};
        }
        else {
            assert(address.sa_family == AF_INET6);
            SOCKADDR_IN6 ipv6Address;
            std::memcpy(&ipv6Address, &address, sizeof(ipv6Address));
            return ipv6_endpoint{ipv6_address{ipv6Address.sin6_addr.u.Byte}, ntohs(ipv6Address.sin6_port)};
        }
    }

    int ip_endpoint_to_sockaddr(const ip_endpoint &endPoint, std::reference_wrapper<sockaddr_storage> address) noexcept
    {
        if (endPoint.is_ipv4()) {
            const auto &ipv4EndPoint = endPoint.to_ipv4();
            SOCKADDR_IN ipv4Address;
            ipv4Address.sin_family = AF_INET;
            std::memcpy(&ipv4Address.sin_addr, ipv4EndPoint.address().bytes(), 4);
            ipv4Address.sin_port = htons(ipv4EndPoint.port());
            std::memset(&ipv4Address.sin_zero, 0, sizeof(ipv4Address.sin_zero));
            std::memcpy(&address.get(), &ipv4Address, sizeof(ipv4Address));
            return sizeof(SOCKADDR_IN);
        }
        else {
            const auto &ipv6EndPoint = endPoint.to_ipv6();
            SOCKADDR_IN6 ipv6Address;
            ipv6Address.sin6_family = AF_INET6;
            std::memcpy(&ipv6Address.sin6_addr, ipv6EndPoint.address().bytes(), 16);
            ipv6Address.sin6_port = htons(ipv6EndPoint.port());
            ipv6Address.sin6_flowinfo = 0;
            ipv6Address.sin6_scope_struct = SCOPEID_UNSPECIFIED_INIT;
            std::memcpy(&address.get(), &ipv6Address, sizeof(ipv6Address));
            return sizeof(SOCKADDR_IN6);
        }
    }

    SOCKET create_socket(int addressFamily, int socketType, int protocol, HANDLE ioCompletionPort)
    {
        // Enumerate available protocol providers for the specified socket type.

        WSAPROTOCOL_INFOW stackInfos[4];
        std::unique_ptr<WSAPROTOCOL_INFOW[]> heapInfos;
        WSAPROTOCOL_INFOW *selectedProtocolInfo = nullptr;

        {
            INT protocols[] = {protocol, 0};
            DWORD bufferSize = sizeof(stackInfos);
            WSAPROTOCOL_INFOW *infos = stackInfos;

            int protocolCount = ::WSAEnumProtocolsW(protocols, infos, &bufferSize);
            if (protocolCount == SOCKET_ERROR) {
                int errorCode = ::WSAGetLastError();
                if (errorCode == WSAENOBUFS) {
                    DWORD requiredElementCount = bufferSize / sizeof(WSAPROTOCOL_INFOW);
                    heapInfos = std::make_unique<WSAPROTOCOL_INFOW[]>(requiredElementCount);
                    bufferSize = requiredElementCount * sizeof(WSAPROTOCOL_INFOW);
                    infos = heapInfos.get();
                    protocolCount = ::WSAEnumProtocolsW(protocols, infos, &bufferSize);
                    if (protocolCount == SOCKET_ERROR) {
                        errorCode = ::WSAGetLastError();
                    }
                }

                if (protocolCount == SOCKET_ERROR) {
                    THROWEXCEPTIONWCODE(errorCode)
                }
            }

            if (protocolCount == 0) {
                throw std::system_error(std::make_error_code(std::errc::protocol_not_supported));
            }

            for (int i = 0; i < protocolCount; ++i) {
                auto &info = infos[i];
                if (info.iAddressFamily == addressFamily && info.iProtocol == protocol && info.iSocketType == socketType) {
                    selectedProtocolInfo = &info;
                    break;
                }
            }

            if (selectedProtocolInfo == nullptr) {
                throw std::system_error(std::make_error_code(std::errc::address_family_not_supported));
            }
        }

        // WSA_FLAG_NO_HANDLE_INHERIT for SDKs earlier than Windows 7.
        constexpr DWORD flagNoInherit = 0x80;

        const DWORD flags = WSA_FLAG_OVERLAPPED | flagNoInherit;

        const SOCKET socketHandle = ::WSASocketW(addressFamily, socketType, protocol, selectedProtocolInfo, 0, flags);
        if (socketHandle == INVALID_SOCKET) {
            THROWEXCEPTION
        }

        // This is needed on operating systems earlier than Windows 7 to prevent
        // socket handles from being inherited. On Windows 7 or later this is
        // redundant as the WSA_FLAG_NO_HANDLE_INHERIT flag passed to creation
        // above causes the socket to be atomically created with this flag cleared.
        if (!::SetHandleInformation((HANDLE)socketHandle, HANDLE_FLAG_INHERIT, 0)) {
            THROWEXCEPTION
        }

        // Associate the socket with the I/O completion port.
        {
            const HANDLE result = ::CreateIoCompletionPort((HANDLE)socketHandle, ioCompletionPort, ULONG_PTR(0), DWORD(0));
            if (result == nullptr) {
                THROWEXCEPTION
            }
        }

        if (socketType == SOCK_STREAM) {
            // Turn off linger so that the destructor doesn't block while closing
            // the socket or silently continue to flush remaining data in the
            // background after ::closesocket() is called, which could fail and
            // we'd never know about it.
            // We expect clients to call Disconnect() or use CloseSend() to cleanly
            // shut-down connections instead.
            BOOL value = TRUE;
            const int result = ::setsockopt(socketHandle, SOL_SOCKET, SO_DONTLINGER, reinterpret_cast<const char *>(&value), sizeof(value));
            if (result == SOCKET_ERROR) {
                THROWEXCEPTION
            }
        }

        return socketHandle;
    }
} // namespace local

socket socket::create_tcpv4(io_service &ioSvc) { return socket(local::create_socket(AF_INET, SOCK_STREAM, IPPROTO_TCP, ioSvc.getHandle())); }

socket socket::create_tcpv6(io_service &ioSvc) { return socket(local::create_socket(AF_INET6, SOCK_STREAM, IPPROTO_TCP, ioSvc.getHandle())); }

socket socket::create_udpv4(io_service &ioSvc) { return socket(local::create_socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP, ioSvc.getHandle())); }

socket socket::create_udpv6(io_service &ioSvc) { return socket(local::create_socket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP, ioSvc.getHandle())); }

socket::socket(socket &&other) noexcept : m_handle(std::exchange(other.m_handle, INVALID_SOCKET)) {}

socket::~socket()
{
    if (m_handle != INVALID_SOCKET) {
        ::closesocket(m_handle);
    }
}

socket &socket::operator=(socket &&other) noexcept
{
    auto handle = std::exchange(other.m_handle, INVALID_SOCKET);
    if (m_handle != INVALID_SOCKET) {
        ::closesocket(m_handle);
    }

    m_handle = handle;
    return *this;
}

ip_endpoint socket::local_endpoint() const
{
    SOCKADDR_STORAGE sockaddrStorage = {0};
    SOCKADDR *sockaddr = reinterpret_cast<SOCKADDR *>(&sockaddrStorage);
    int sockaddrLen = sizeof(sockaddrStorage);
    auto result = ::getsockname(m_handle, sockaddr, &sockaddrLen);
    if (result == 0) {
        return local::sockaddr_to_ip_endpoint(*sockaddr);
    }
}

ip_endpoint socket::remote_endpoint() const
{
    SOCKADDR_STORAGE sockaddrStorage = {0};
    SOCKADDR *sockaddr = reinterpret_cast<SOCKADDR *>(&sockaddrStorage);
    int sockaddrLen = sizeof(sockaddrStorage);
    auto result = ::getpeername(m_handle, sockaddr, &sockaddrLen);
    if (result == 0) {
        return local::sockaddr_to_ip_endpoint(*sockaddr);
    }
}

void socket::bind(const ip_endpoint &localEndPoint)
{
    SOCKADDR_STORAGE sockaddrStorage = {0};
    SOCKADDR *sockaddr = reinterpret_cast<SOCKADDR *>(&sockaddrStorage);
    if (localEndPoint.is_ipv4()) {
        SOCKADDR_IN &ipv4Sockaddr = *reinterpret_cast<SOCKADDR_IN *>(sockaddr);
        ipv4Sockaddr.sin_family = AF_INET;
        std::memcpy(&ipv4Sockaddr.sin_addr, localEndPoint.to_ipv4().address().bytes(), 4);
        ipv4Sockaddr.sin_port = localEndPoint.to_ipv4().port();
    }
    else {
        SOCKADDR_IN6 &ipv6Sockaddr = *reinterpret_cast<SOCKADDR_IN6 *>(sockaddr);
        ipv6Sockaddr.sin6_family = AF_INET6;
        std::memcpy(&ipv6Sockaddr.sin6_addr, localEndPoint.to_ipv6().address().bytes(), 16);
        ipv6Sockaddr.sin6_port = localEndPoint.to_ipv6().port();
    }

    int result = ::bind(m_handle, sockaddr, sizeof(sockaddrStorage));
    if (result != 0) {
        THROWEXCEPTION
    }
}

void socket::listen() { listen(SOMAXCONN); }

void socket::listen(std::uint32_t backlog)
{
    if (backlog > SOMAXCONN) {
        backlog = SOMAXCONN;
    }

    auto result = ::listen(m_handle, (int)backlog);
    if (result != 0) {
        THROWEXCEPTION
    }
}

socket_accept_operation socket::accept(socket &acceptingSocket) noexcept { return socket_accept_operation{*this, acceptingSocket}; }

socket_connect_operation socket::connect(const ip_endpoint &remoteEndPoint) noexcept { return socket_connect_operation{*this, remoteEndPoint}; }

socket_disconnect_operation socket::disconnect() noexcept { return socket_disconnect_operation(*this); }

socket_send_operation socket::send(const void *buffer, std::size_t byteCount) noexcept { return socket_send_operation{*this, buffer, byteCount}; }

socket_recv_operation socket::recv(void *buffer, std::size_t byteCount) noexcept { return socket_recv_operation{*this, buffer, byteCount}; }

socket_recv_from_operation socket::recv_from(void *buffer, std::size_t byteCount) noexcept
{
    return socket_recv_from_operation{*this, buffer, byteCount};
}

socket_send_to_operation socket::send_to(const ip_endpoint &destination, const void *buffer, std::size_t byteCount) noexcept
{
    return socket_send_to_operation{*this, destination, buffer, byteCount};
}
socket::socket(win32::socket_t handle) noexcept : m_handle(handle) {}
} // namespace SL::Network