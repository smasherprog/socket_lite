#include "Context.h"
#include "Socket.h"
#include <assert.h>

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
    SocketStatus Socket::get_status() const { return SocketStatus_; }
    std::string Socket::get_address() const { return std::string(); }
    unsigned short Socket::get_port() const { return 0; }
    NetworkProtocol Socket::get_protocol() const { return NetworkProtocol(); }
    bool Socket::is_loopback() const { return false; }

    void Socket::close()
    {
        if (SocketStatus_ == SocketStatus::CLOSED) {
            return; // nothing more to do here!
        }
        if (handle != INVALID_SOCKET) {
            closesocket(handle);
        }

        handle = INVALID_SOCKET;
        SocketStatus_ = SocketStatus::CLOSED;
        // let all handlers know
        std::list<PER_IO_CONTEXT> tmp;
        {
            std::lock_guard<std::mutex> lock(SendBuffersLock);
            std::swap(tmp, SendBuffers);
        }
        for (auto &a : tmp) {
            a.completionhandler(SOCKETCLOSED); // complete all handlers leting them know of the disconnect
        }
        tmp.clear();
        if (ReadContext) {
            ReadContext->completionhandler(SOCKETCLOSED);
            ReadContext.reset();
        }
        Parent->onDisconnection(shared_from_this());
    }
    void Socket::async_read(size_t buffer_size, unsigned char *buffer, const std::function<void(size_t)> &handler)
    {
        assert(!ReadContext);
        ReadContext = std::make_unique<PER_IO_CONTEXT>();
        ReadContext->Socket_ = std::static_pointer_cast<Socket>(shared_from_this());
        ReadContext->buffer = buffer;
        ReadContext->bufferlen = buffer_size;
        ReadContext->completionhandler = handler;
        ReadContext->IOOperation = IO_OPERATION::IoRead;

        ReadContext->wsabuf.buf = nullptr;
        ReadContext->wsabuf.len = 0;
        DWORD dwSendNumBytes(0), dwFlags(0);
        // do read with zero bytes to prevent memory from being mapped
        if (auto nRet = WSARecv(handle, &ReadContext->wsabuf, 1, &dwSendNumBytes, &dwFlags, &(ReadContext->Overlapped), NULL);
            nRet == SOCKET_ERROR && (ERROR_IO_PENDING != WSAGetLastError())) {
            close();
        }
    }
    SocketStatus Socket::continue_read(PER_IO_CONTEXT *sockcontext)
    {
        // read any data available
        if (sockcontext->bufferlen < sockcontext->transfered_bytes) {
            auto bytes_read = 0;
            do {
                auto remainingbytes = ReadContext->bufferlen - ReadContext->transfered_bytes;
                bytes_read = recv(handle, (char *)ReadContext->buffer + ReadContext->transfered_bytes, remainingbytes, 0);
                if (bytes_read > 0) {
                    ReadContext->transfered_bytes += bytes_read;
                }
                else if (bytes_read == 0 || (bytes_read == SOCKET_ERROR && WSAGetLastError() != WSAEWOULDBLOCK)) {
                    close();
                    return get_status();
                }
            } while (bytes_read > 0);
        }

        if (sockcontext->bufferlen == sockcontext->transfered_bytes) {
            sockcontext->completionhandler(sockcontext->transfered_bytes);
            ReadContext.reset();
        }
        else {
            // still more data to read.. keep going!
            ReadContext->wsabuf.buf = nullptr;
            ReadContext->wsabuf.len = 0;
            DWORD dwSendNumBytes(0), dwFlags(0);
            // do read with zero bytes to prevent memory from being mapped
            if (auto nRet = WSARecv(handle, &ReadContext->wsabuf, 1, &dwSendNumBytes, &dwFlags, &(ReadContext->Overlapped), NULL);
                nRet == SOCKET_ERROR && (ERROR_IO_PENDING != WSAGetLastError())) {
                close();
            }
        }
        return get_status();
    }
    void Socket::async_write(size_t buffer_size, unsigned char *buffer, const std::function<void(size_t)> &handler)
    {
        if (SocketStatus_ != SocketStatus::CONNECTED) {
            handler(SOCKETCLOSED);
        }
        size_t size = 0;
        {
            std::lock_guard<std::mutex> lock(SendBuffersLock);
            auto val = SendBuffers.emplace_back(PER_IO_CONTEXT());
            val.buffer = buffer;
            val.bufferlen = buffer_size;
            val.IOOperation = IO_OPERATION::IoWrite;
            size = SendBuffers.size();
        }
        if (size == 1) { // initiate a send
            continue_write(&SendBuffers.front());
        }
    }
    SocketStatus Socket::continue_write(PER_IO_CONTEXT *sockcontext)
    {
        // check if the send is done, if so callback, remove from the send buffer, etc
        if (sockcontext->transfered_bytes == sockcontext->bufferlen) {
            {
                std::lock_guard<std::mutex> lock(SendBuffersLock);
                SendBuffers.pop_front();
                if (SendBuffers.begin() != SendBuffers.end()) { // dont use empty as it traverses the list
                    sockcontext = &SendBuffers.front();
                }
            }
            sockcontext->completionhandler(sockcontext->transfered_bytes);
        }
        if (sockcontext == nullptr) {
            return get_status();
        }
        sockcontext->wsabuf.buf = (char *)sockcontext->buffer + sockcontext->transfered_bytes;
        sockcontext->wsabuf.len = sockcontext->bufferlen - sockcontext->transfered_bytes;
        DWORD dwSendNumBytes(0), dwFlags(0);
        if (auto nRet = WSASend(handle, &sockcontext->wsabuf, 1, &dwSendNumBytes, dwFlags, &(sockcontext->Overlapped), NULL);
            nRet == SOCKET_ERROR && (ERROR_IO_PENDING != WSAGetLastError())) {
            close();
        }
        return get_status();
    }

} // namespace NET
} // namespace SL
