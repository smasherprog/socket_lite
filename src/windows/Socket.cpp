
#include "Socket.h"
#include <assert.h>

namespace SL {
namespace NET {

    Socket::Socket() {}
    Socket::~Socket() { close(); }

    void Socket::close()
    {
        if (handle == INVALID_SOCKET) {
            return; // nothing more to do here!
        }
        auto tmphandle = INVALID_SOCKET;
        {
            std::lock_guard<SpinLock> lock(Lock);
            if (handle == INVALID_SOCKET) {
                return; // nothing more to do here!
            }
            tmphandle = handle;
            handle = INVALID_SOCKET;
        }
        closesocket(handle);
    }
    void Socket::async_read(size_t buffer_size, unsigned char *buffer, const std::function<void(Bytes_Transfered)> &&handler)
    {
        assert(!ReadContext);
        if (handle == INVALID_SOCKET) {
            handler(-1);
        }
        ReadContext = new PER_IO_CONTEXT();
        ReadContext->Socket_ = shared_from_this();
        ReadContext->buffer = buffer;
        ReadContext->bufferlen = buffer_size;
        ReadContext->completionhandler = std::move(handler);
        ReadContext->IOOperation = IO_OPERATION::IoRead;
        ReadContext->wsabuf.buf = nullptr;
        ReadContext->wsabuf.len = 0;
        DWORD dwSendNumBytes(0), dwFlags(0);
        // do read with zero bytes to prevent memory from being mapped
        if (auto nRet = WSARecv(handle, &ReadContext->wsabuf, 1, &dwSendNumBytes, &dwFlags, &(ReadContext->Overlapped), NULL);
            nRet == SOCKET_ERROR && (ERROR_IO_PENDING != WSAGetLastError())) {
            return read_complete(-1, ReadContext);
        }
    }
    void Socket::read_complete(Bytes_Transfered b, PER_IO_CONTEXT *sockcontext)
    {
        if (b == -1) {
            close();
        }
        assert(sockcontext == ReadContext);
        if (ReadContext) {
            ReadContext->completionhandler(-1); // let the caller know
            delete ReadContext;
            ReadContext = nullptr;
        }
    }
    PER_IO_CONTEXT *Socket::write_complete(Bytes_Transfered b, PER_IO_CONTEXT *sockcontext)
    {
        if (b == -1) {
            close();
            std::list<PER_IO_CONTEXT *> tmpbuf;
            {
                std::lock_guard<SpinLock> lock(Lock);
                tmpbuf = std::move(SendBuffers);
            }
            assert(sockcontext == tmpbuf.front());
            while (!tmpbuf.empty()) {
                tmpbuf.front()->completionhandler(-1);
                delete tmpbuf.front();
                tmpbuf.pop_front();
            }
        }
        else {
            sockcontext->completionhandler(b);
            delete sockcontext;
            std::lock_guard<SpinLock> lock(Lock);
            assert(sockcontext == SendBuffers.front());
            SendBuffers.pop_front();
            if (SendBuffers.begin() != SendBuffers.end()) { // dont use empty as it traverses the list
                return SendBuffers.front();
            }
        }
        return nullptr;
    }

    void Socket::continue_read(PER_IO_CONTEXT *sockcontext)
    {
        assert(ReadContext == sockcontext);
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
                    return read_complete(-1, sockcontext);
                }
            } while (bytes_read > 0);
        }

        if (sockcontext->bufferlen == sockcontext->transfered_bytes) {
            return read_complete(sockcontext->transfered_bytes, sockcontext);
        }
        else {
            // still more data to read.. keep going!
            ReadContext->wsabuf.buf = nullptr;
            ReadContext->wsabuf.len = 0;
            DWORD dwSendNumBytes(0), dwFlags(0);
            // do read with zero bytes to prevent memory from being mapped
            if (auto nRet = WSARecv(handle, &ReadContext->wsabuf, 1, &dwSendNumBytes, &dwFlags, &(ReadContext->Overlapped), NULL);
                nRet == SOCKET_ERROR && (ERROR_IO_PENDING != WSAGetLastError())) {
                return read_complete(-1, sockcontext);
            }
        }
    }
    void Socket::async_write(size_t buffer_size, unsigned char *buffer, const std::function<void(Bytes_Transfered)> &&handler)
    {
        bool sendbufferempty = false;
        {
            std::lock_guard<SpinLock> lock(Lock);
            auto val = SendBuffers.emplace_back(new PER_IO_CONTEXT());
            val->Socket_ = shared_from_this();
            val->buffer = buffer;
            val->bufferlen = buffer_size;
            val->completionhandler = handler;
            val->IOOperation = IO_OPERATION::IoWrite;
            sendbufferempty = SendBuffers.begin() == SendBuffers.end();
        }
        if (sendbufferempty) { // initiate a send
            continue_write(SendBuffers.front());
        }
    }
    void Socket::continue_write(PER_IO_CONTEXT *sockcontext)
    {
        // check if the send is done, if so callback, remove from the send buffer, etc
        if (sockcontext->transfered_bytes == sockcontext->bufferlen) {
            sockcontext = write_complete(sockcontext->transfered_bytes, sockcontext);
        }
        if (sockcontext != nullptr) {
            sockcontext->wsabuf.buf = (char *)sockcontext->buffer + sockcontext->transfered_bytes;
            sockcontext->wsabuf.len = sockcontext->bufferlen - sockcontext->transfered_bytes;
            DWORD dwSendNumBytes(0), dwFlags(0);
            if (auto nRet = WSASend(handle, &sockcontext->wsabuf, 1, &dwSendNumBytes, dwFlags, &(sockcontext->Overlapped), NULL);
                nRet == SOCKET_ERROR && (ERROR_IO_PENDING != WSAGetLastError())) {
                write_complete(-1, sockcontext);
            }
        }
    }

} // namespace NET
} // namespace SL
