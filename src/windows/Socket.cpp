
#include "Context.h"
#include "Socket.h"
#include <assert.h>

namespace SL {
namespace NET {

    Socket::Socket(Context *context, AddressFamily family) : Socket(context) { handle = INTERNAL::Socket(family); }
    Socket::Socket(Context *context) : Context_(context) {}
    Socket::~Socket() {}

    bool Socket::UpdateIOCP(SOCKET socket, HANDLE *iocp, void *completionkey)
    {
        if (*iocp = CreateIoCompletionPort((HANDLE)socket, *iocp, (DWORD_PTR)completionkey, 0); *iocp != NULL) {
            return true;
        }
        else {
            return false;
        }
    }

    void Socket::recv(size_t buffer_size, unsigned char *buffer, std::function<void(StatusCode, size_t)> &&handler)
    {
        auto context = Context_->Win_IO_RW_ContextBuffer.newObject();
        context->buffer = buffer;
        context->bufferlen = buffer_size;
        context->completionhandler = Context_->RW_CompletionHandlerBuffer.newObject();
        context->completionhandler->completionhandler = std::move(handler);
        context->completionhandler->RefCount = 1;
        context->IOOperation = IO_OPERATION::IoRead;
        continue_io(true, context);
    }

    void Socket::continue_io(bool success, Win_IO_RW_Context *context)
    {
        if (!success) {
            close();
            context->completionhandler->handle(TranslateError(), 0, true);
            auto c = Context_; // take a copy
            if (context->completionhandler->RefCount.fetch_sub(1, std::memory_order_relaxed) == 1) {
                Context_->RW_CompletionHandlerBuffer.deleteObject(context->completionhandler);
            }
            c->Win_IO_RW_ContextBuffer.deleteObject(context);
        }
        else if (context->bufferlen == context->transfered_bytes) {
            context->completionhandler->handle(StatusCode::SC_SUCCESS, context->transfered_bytes, true);
            auto c = Context_; // take a copy
            if (context->completionhandler->RefCount.fetch_sub(1, std::memory_order_relaxed) == 1) {
                Context_->RW_CompletionHandlerBuffer.deleteObject(context->completionhandler);
            }
            c->Win_IO_RW_ContextBuffer.deleteObject(context);
        }
        else {
            auto func = context->completionhandler;
            func->RefCount.fetch_add(1, std::memory_order_relaxed);
            WSABUF wsabuf;
            auto bytesleft = static_cast<decltype(wsabuf.len)>(context->bufferlen - context->transfered_bytes);
            wsabuf.buf = (char *)context->buffer + context->transfered_bytes;
            wsabuf.len = bytesleft;
            DWORD dwSendNumBytes(0), dwFlags(0);
            Context_->PendingIO += 1;
            DWORD nRet = 0;
            if (context->IOOperation == IO_OPERATION::IoRead) {
                nRet = WSARecv(handle, &wsabuf, 1, &dwSendNumBytes, &dwFlags, &(context->Overlapped), NULL);
            }
            else {
                nRet = WSASend(handle, &wsabuf, 1, &dwSendNumBytes, dwFlags, &(context->Overlapped), NULL);
            }
            auto lasterr = WSAGetLastError();
            if (nRet == SOCKET_ERROR && (WSA_IO_PENDING != lasterr)) {
                Context_->PendingIO -= 1;
                close();
                func->handle(TranslateError(&lasterr), 0, false);
                auto c = Context_; // take a copy
                c->RW_CompletionHandlerBuffer.deleteObject(func);
                c->Win_IO_RW_ContextBuffer.deleteObject(context);
            }
            else if (nRet == 0 && dwSendNumBytes == bytesleft) {
                func->handle(StatusCode::SC_SUCCESS, bytesleft, true);
                if (func->RefCount.fetch_sub(1, std::memory_order_relaxed) == 1) {
                    Context_->RW_CompletionHandlerBuffer.deleteObject(func);
                }
            }
            else {
                if (func->RefCount.fetch_sub(1, std::memory_order_relaxed) == 1) {
                    Context_->RW_CompletionHandlerBuffer.deleteObject(func);
                }
            }
        }
    }
    void BindSocket(SOCKET sock, AddressFamily family)
    {
        if (family == AddressFamily::IPV4) {
            sockaddr_in bindaddr = {0};
            bindaddr.sin_family = AF_INET;
            bindaddr.sin_addr.s_addr = INADDR_ANY;
            bindaddr.sin_port = 0;
            sockaddr mytestaddr((unsigned char *)&bindaddr, sizeof(bindaddr), "", 0, AddressFamily::IPV4);
            INTERNAL::bind(sock, mytestaddr);
        }
        else {
            sockaddr_in6 bindaddr = {0};
            bindaddr.sin6_family = AF_INET6;
            bindaddr.sin6_addr = in6addr_any;
            bindaddr.sin6_port = 0;
            sockaddr mytestaddr((unsigned char *)&bindaddr, sizeof(bindaddr), "", 0, AddressFamily::IPV6);
            INTERNAL::bind(sock, mytestaddr);
        }
    }
    void Socket::init_connect(bool success, Win_IO_Connect_Context *context)
    {
        static auto iodone = [](auto code, auto context) {
            context->completionhandler(code);
            delete context;
        };
        if (!success) {
            return iodone(TranslateError(), context);
        }
        ISocket::close();
        handle = INTERNAL::Socket(context->address.get_Family());
        BindSocket(handle, context->address.get_Family());
        if (!setsockopt<SocketOptions::O_BLOCKING>(Blocking_Options::NON_BLOCKING)) {
            return iodone(TranslateError(), context);
        }

        if (!Socket::UpdateIOCP(handle, &Context_->iocp.handle, this)) {
            return iodone(TranslateError(), context);
        }
        context->IOOperation = IO_OPERATION::IoConnect;
        context->Overlapped = {0};
        Context_->PendingIO += 1;
        auto connectres = Context_->ConnectEx_(handle, (::sockaddr *)context->address.get_SocketAddr(), context->address.get_SocketAddrLen(), 0, 0, 0,
                                               (LPOVERLAPPED)&context->Overlapped);
        if (connectres == TRUE) {
            Context_->PendingIO -= 1;
            iodone(StatusCode::SC_SUCCESS, context);
        }
        else if (auto err = WSAGetLastError(); !(connectres == FALSE && err == ERROR_IO_PENDING)) {
            Context_->PendingIO -= 1;
            iodone(TranslateError(&err), context);
        }
    }

    void Socket::continue_connect(bool success, Win_IO_Connect_Context *context)
    {
        static auto iodone = [](auto code, auto context) {
            context->completionhandler(code);
            delete context;
        };
        if (success) {
            if (::setsockopt(handle, SOL_SOCKET, SO_UPDATE_CONNECT_CONTEXT, 0, 0) == SOCKET_ERROR) {
                iodone(TranslateError(), context);
            }
            else {
                iodone(StatusCode::SC_SUCCESS, context);
            }
        }
        else {
            iodone(TranslateError(), context);
        }
    }
    void Socket::connect(sockaddr &address, const std::function<void(StatusCode)> &&handler)
    {
        auto iofailed = [](auto code, auto context) {
            auto chandle(std::move(context->completionhandler));
            delete context;
            chandle(code);
        };
        auto context = new Win_IO_Connect_Context();
        context->completionhandler = std::move(handler);
        context->IOOperation = IO_OPERATION::IoInitConnect;
        context->Socket_ = this;
        context->address = address;
        if (Context_->inWorkerThread()) {
            init_connect(true, context);
        }
        else {
            Context_->PendingIO += 1;
            if (PostQueuedCompletionStatus(Context_->iocp.handle, 0, (ULONG_PTR)this, (LPOVERLAPPED)&context->Overlapped) == FALSE) {
                Context_->PendingIO -= 1;
                iofailed(TranslateError(), context);
            }
        }
    }
    void Socket::send(size_t buffer_size, unsigned char *buffer, std::function<void(StatusCode, size_t)> &&handler)
    {
        auto context = Context_->Win_IO_RW_ContextBuffer.newObject();
        context->buffer = buffer;
        context->bufferlen = buffer_size;
        context->completionhandler = Context_->RW_CompletionHandlerBuffer.newObject();
        context->completionhandler->completionhandler = std::move(handler);
        context->completionhandler->RefCount = 1;
        context->IOOperation = IO_OPERATION::IoWrite;
        continue_io(true, context);
    }

} // namespace NET
} // namespace SL
