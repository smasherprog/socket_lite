#include "Context.h"
#include "Socket.h"
#include "internal/FastAllocator.h"

namespace SL {
namespace NET {
    SOCKET Socket::Create(NetworkProtocol protocol)
    {
        auto type = protocol == NetworkProtocol::IPV4 ? AF_INET : AF_INET6;
        if (auto handle = WSASocketW(type, SOCK_STREAM, IPPROTO_IP, NULL, 0, WSA_FLAG_OVERLAPPED); handle != INVALID_SOCKET) {
            auto nZero = 0;
            if (auto nRet = setsockopt(handle, SOL_SOCKET, SO_SNDBUF, (char *)&nZero, sizeof(nZero)); nRet == SOCKET_ERROR) {
                closesocket(handle);
                return INVALID_SOCKET;
            }
            u_long iMode = 1; // set socket for non blocking
            if (auto nRet = ioctlsocket(handle, FIONBIO, &iMode); nRet == NO_ERROR) {
                closesocket(handle);
                return INVALID_SOCKET;
            }
            return handle;
        }
        return INVALID_SOCKET;
    }
    Socket::Socket(NetworkProtocol protocol) { handle = Create(protocol); }
    Socket::~Socket() { close(); }
    SocketStatus Socket::is_open() const { return SocketStatus(); }
    std::string Socket::get_address() const { return std::string(); }
    unsigned short Socket::get_port() const { return 0; }
    NetworkProtocol Socket::get_protocol() const { return NetworkProtocol(); }
    bool Socket::is_loopback() const { return false; }
    size_t Socket::BufferedBytes() const { return size_t(); }
    void Socket::send(const Message &msg)
    {
        bool dosend = false;
        {
            std::lock_guard<std::mutex> lock(PER_IO_CONTEXT_->SendBuffersLock);
            // still sennding other messages, just push
            if (!PER_IO_CONTEXT_->SendBuffers.empty() || !PER_IO_CONTEXT_->CurrentBuffer.Buffer) {
                PER_IO_CONTEXT_->SendBuffers.emplace_back(0, msg.len, msg.Buffer);
            }
            else {
                dosend = true;
                PER_IO_CONTEXT_->CurrentBuffer.Buffer = msg.Buffer;
                PER_IO_CONTEXT_->CurrentBuffer.total = msg.len;
                PER_IO_CONTEXT_->CurrentBuffer.totalsent = 0;
            }
        }
        if (dosend) {
            if (!send()) {
                Context_->onDisconnection(PER_IO_CONTEXT_->Socket);
                freecontext(&PER_IO_CONTEXT_); // will this work??? Who knows lets try, it should!
            }
        }
    }
    void Socket::close()
    {
        if (handle != INVALID_SOCKET) {
            closesocket(handle);
        }
        handle = INVALID_SOCKET;
    }

    bool Socket::read(FastAllocator &allocator)
    {
        auto bytes_read = 0;
        do {
            auto remainingbytes = allocator.capacity() - allocator.size();
            if (remainingbytes < allocator.size()) {
                allocator.grow();
                remainingbytes = allocator.capacity() - allocator.size();
            }
            bytes_read = recv(handle, (char *)allocator.data() + allocator.size(), remainingbytes, 0);
            if (bytes_read > 0) {
                allocator.size(allocator.size() + bytes_read);
            }
            else if (bytes_read == 0 || (bytes_read == SOCKET_ERROR && WSAGetLastError() != WSAEWOULDBLOCK)) {
                return false; // either disconnect or error has occured
            }
        } while (bytes_read > 0);
        return true;
    }

    bool Socket::send()
    {
        bool dosend = true;
        if (!PER_IO_CONTEXT_->CurrentBuffer.Buffer) {
            std::lock_guard<std::mutex> lock(PER_IO_CONTEXT_->SendBuffersLock);
            if (!PER_IO_CONTEXT_->CurrentBuffer.Buffer && !PER_IO_CONTEXT_->SendBuffers.empty()) {
                PER_IO_CONTEXT_->CurrentBuffer.Buffer = msg.Buffer;
                PER_IO_CONTEXT_->CurrentBuffer.total = msg.len;
                PER_IO_CONTEXT_->CurrentBuffer.totalsent = 0;
            }
            else {
                dosend = false;
            }
        }

        PER_IO_CONTEXT_->wsabuf.buf = (char *)PER_IO_CONTEXT_->CurrentBuffer.Buffer.get() + PER_IO_CONTEXT_->CurrentBuffer.totalsent;
        PER_IO_CONTEXT_->wsabuf.len = PER_IO_CONTEXT_->CurrentBuffer.total - PER_IO_CONTEXT_->CurrentBuffer.totalsent;

        DWORD dwSendNumBytes(0), dwFlags(0);
        if (auto nRet =
                WSASend(PER_IO_CONTEXT_->Socket->handle, &PER_IO_CONTEXT_->wsabuf, 1, &dwSendNumBytes, dwFlags, &(PER_IO_CONTEXT_->Overlapped), NULL);
            nRet == SOCKET_ERROR && (ERROR_IO_PENDING != WSAGetLastError())) {
            Context_->onDisconnection(PER_IO_CONTEXT_->Socket);
            freecontext(&PER_IO_CONTEXT_); // will this work??? Who knows lets try, it should!
        }

        DWORD dwSendNumBytes(0), dwFlags(0);
        if (PER_IO_CONTEXT_->nSentBytes < PER_IO_CONTEXT_->nTotalBytes) {
            PER_IO_CONTEXT_->wsabuf.buf = (char *)PER_IO_CONTEXT_->Buffer.get() + PER_IO_CONTEXT_->nSentBytes;
            PER_IO_CONTEXT_->wsabuf.len = PER_IO_CONTEXT_->nTotalBytes - PER_IO_CONTEXT_->nSentBytes;
            if (auto nRet = WSASend(PER_IO_CONTEXT_->Socket->handle, &PER_IO_CONTEXT_->wsabuf, 1, &dwSendNumBytes, dwFlags,
                                    &(PER_IO_CONTEXT_->Overlapped), NULL);
                nRet == SOCKET_ERROR && (ERROR_IO_PENDING != WSAGetLastError())) {
                return false;
            }
        }
        else {
            PER_IO_CONTEXT_->Buffer.reset(); // release memory
            PER_IO_CONTEXT_->wsabuf.len = PER_IO_CONTEXT_->nTotalBytes = PER_IO_CONTEXT_->nSentBytes = 0;
            PER_IO_CONTEXT_->wsabuf.buf = nullptr;
        }
        return true;
    }

} // namespace NET
} // namespace SL
