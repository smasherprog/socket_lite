
#include "IO_Context.h"
#include "Socket.h"
#include "common/List.h"
#include <assert.h>

namespace SL {
namespace NET {

    std::shared_ptr<ISocket> SOCKET_LITE_EXTERN CreateSocket() { return std::make_shared<Socket>(); }

    Socket::Socket() { handle = INVALID_SOCKET; }
    Socket::~Socket()
    { // make sure the links are all cleaned up
        close();
    }

    bool Socket::UpdateIOCP(SOCKET socket, HANDLE *iocp, void *completionkey)
    {
        if (*iocp = CreateIoCompletionPort((HANDLE)socket, *iocp, (DWORD_PTR)completionkey, 0); *iocp != NULL) {
            return true;
        }
        else {
            std::cerr << "CreateIoCompletionPort() failed: " << GetLastError() << std::endl;
            return false;
        }
    }

    void Socket::close()
    {
        if (handle != INVALID_SOCKET) {
            closesocket(handle);
        }
        handle = INVALID_SOCKET;
    }
    void Socket::async_connect(const std::shared_ptr<IIO_Context> &io_context, sockaddr addr, const std::function<void(Bytes_Transfered)> &&handler)
    {
        auto iocontext = static_cast<IO_Context *>(io_context.get());
        GUID guid = WSAID_CONNECTEX;
        DWORD bytes = 0;
        LPFN_CONNECTEX connectex;
        if (addr.Family == Address_Family::IPV4) {
            handle = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0, WSA_FLAG_OVERLAPPED);
        }
        else {
            handle = WSASocket(AF_INET6, SOCK_STREAM, IPPROTO_TCP, NULL, 0, WSA_FLAG_OVERLAPPED);
        }
        if (!bind(addr)) {
            handler(-1);
            return;
        }
        if (WSAIoctl(handle, SIO_GET_EXTENSION_FUNCTION_POINTER, &guid, sizeof(guid), &connectex, sizeof(connectex), &bytes, NULL, NULL) !=
            SOCKET_ERROR) {
            if (iocontext->iocp.handle = CreateIoCompletionPort((HANDLE)handle, iocontext->iocp.handle, (DWORD_PTR)this, 0);
                iocontext->iocp.handle == NULL) {
                std::cerr << "CreateIoCompletionPort() failed: " << GetLastError() << std::endl;
                handler(-1);
                return;
            }
        }
        else {
            std::cerr << "failed to load ConnectEX: " << WSAGetLastError() << std::endl;
            handler(-1);
            return;
        }
        ReadContext.clear();
        ReadContext.IO_Context.completionhandler = std::move(handler);
        ReadContext.IOOperation = IO_OPERATION::IoConnect;

        if (addr.Family == Address_Family::IPV4) {
            ::sockaddr_in sockaddr = {0};
            int sockaddrlen = sizeof(sockaddr);
            inet_pton(AF_INET, addr.Address, &sockaddr.sin_addr);
            DWORD bytessend = 0;
            if (auto connectres = connectex(handle, (::sockaddr *)&sockaddr, sockaddrlen, NULL, 0, &bytessend, (LPOVERLAPPED)&ReadContext);
                connectres == TRUE || (connectres == FALSE && WSAGetLastError() == ERROR_IO_PENDING)) {
                return;
            }
        }
        else {
            sockaddr_in6 sockaddr = {0};
            int sockaddrlen = sizeof(sockaddr);
            inet_pton(AF_INET6, addr.Address, &sockaddr.sin6_addr);
            DWORD bytessend = 0;
            if (auto connectres = connectex(handle, (::sockaddr *)&sockaddr, sockaddrlen, NULL, 0, &bytessend, (LPOVERLAPPED)&ReadContext);
                connectres == TRUE || (connectres == FALSE && WSAGetLastError() == ERROR_IO_PENDING)) {
                return;
            }
        }
        handler(-1);
        ReadContext.clear();
    }
    void Socket::async_read(size_t buffer_size, unsigned char *buffer, const std::function<void(Bytes_Transfered)> &&handler)
    {
        if (handle == INVALID_SOCKET) {
            handler(-1);
        }
        assert(!ReadContext);
        ReadContext = std::make_unique<Win_IO_Context>();
        ReadContext->IO_Context.Socket_ = shared_from_this();
        ReadContext->IO_Context.buffer = buffer;
        ReadContext->IO_Context.bufferlen = buffer_size;
        ReadContext->IO_Context.completionhandler = std::move(handler);
        ReadContext->IOOperation = IO_OPERATION::IoRead;
        ReadContext->wsabuf.buf = nullptr;
        ReadContext->wsabuf.len = 0;
        DWORD dwSendNumBytes(0), dwFlags(0);
        // do read with zero bytes to prevent memory from being mapped
        if (auto nRet = WSARecv(handle, &ReadContext->wsabuf, 1, &dwSendNumBytes, &dwFlags, &(ReadContext->Overlapped), NULL);
            nRet == SOCKET_ERROR && (ERROR_IO_PENDING != WSAGetLastError())) {
            return read_complete(-1, ReadContext.get());
        }
    }
    void Socket::read_complete(Bytes_Transfered b, Win_IO_Context *sockcontext)
    {
        if (b == -1) {
            close();
        }
        assert(sockcontext == ReadContext.get());
        auto tmpreadcontext = std::move(ReadContext);
        sockcontext->IO_Context.completionhandler(b); // let the caller know
    }
    Win_IO_Context_List *Socket::write_complete(Bytes_Transfered b, Win_IO_Context_List *sockcontext)
    {
        if (b == -1) {
            close();
            Win_IO_Context_List *head;
            {
                std::lock_guard<SpinLock> lock(WriteContextsLock);
                head = WriteContexts;
                WriteContexts = nullptr;
            }
            while (head) { // delete all writes and let callers know of the cancelation
                head->IO_Context.completionhandler(-1);
                auto nextcontext = head->next;
                delete head;
                head = nextcontext;
            }
            return nullptr;
        }
        else {
            sockcontext->IO_Context.completionhandler(b);
            assert(sockcontext == WriteContexts);
            Win_IO_Context_List *ret = nullptr;
            {
                std::lock_guard<SpinLock> lock(WriteContextsLock);
                pop_front(&WriteContexts);
                ret = WriteContexts;
            }
            delete sockcontext;
            return ret;
        }
    }

    void Socket::continue_read(Win_IO_Context *sockcontext)
    {
        assert(ReadContext.get() == sockcontext);
        // read any data available
        if (sockcontext->IO_Context.bufferlen < sockcontext->IO_Context.transfered_bytes) {
            auto bytes_read = 0;
            do {
                auto remainingbytes = ReadContext->IO_Context.bufferlen - ReadContext->IO_Context.transfered_bytes;
                bytes_read = recv(handle, (char *)ReadContext->IO_Context.buffer + ReadContext->IO_Context.transfered_bytes, remainingbytes, 0);
                if (bytes_read > 0) {
                    ReadContext->IO_Context.transfered_bytes += bytes_read;
                }
                else if (bytes_read == 0 || (bytes_read == SOCKET_ERROR && WSAGetLastError() != WSAEWOULDBLOCK)) {
                    return read_complete(-1, sockcontext);
                }
            } while (bytes_read > 0);
        }

        if (sockcontext->IO_Context.bufferlen == sockcontext->IO_Context.transfered_bytes) {
            return read_complete(sockcontext->IO_Context.transfered_bytes, sockcontext);
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
        auto val = new Win_IO_Context_List();
        val->IO_Context.Socket_ = shared_from_this();
        val->IO_Context.buffer = buffer;
        val->IO_Context.bufferlen = buffer_size;
        val->IO_Context.completionhandler = handler;
        val->IOOperation = IO_OPERATION::IoWrite;
        {
            std::lock_guard<SpinLock> lock(WriteContextsLock);
            sendbufferempty = WriteContexts == nullptr;
            append(&WriteContexts, val);
        }
        if (sendbufferempty) { // initiate a send
            continue_write(val);
        }
    }
    void Socket::continue_write(Win_IO_Context_List *sockcontext)
    {
        // check if the send is done, if so callback, remove from the send buffer, etc
        if (sockcontext->IO_Context.transfered_bytes == sockcontext->IO_Context.bufferlen) {
            sockcontext = write_complete(sockcontext->IO_Context.transfered_bytes, sockcontext);
        }
        if (sockcontext != nullptr) {
            sockcontext->wsabuf.buf = (char *)sockcontext->IO_Context.buffer + sockcontext->IO_Context.transfered_bytes;
            sockcontext->wsabuf.len = sockcontext->IO_Context.bufferlen - sockcontext->IO_Context.transfered_bytes;
            DWORD dwSendNumBytes(0), dwFlags(0);
            if (auto nRet = WSASend(handle, &sockcontext->wsabuf, 1, &dwSendNumBytes, dwFlags, &(sockcontext->Overlapped), NULL);
                nRet == SOCKET_ERROR && (ERROR_IO_PENDING != WSAGetLastError())) {
                write_complete(-1, sockcontext);
            }
        }
    }

} // namespace NET
} // namespace SL
