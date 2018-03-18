
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
        auto context = Context_->Win_IO_RW_ContextAllocator.allocate(1);
        context->buffer = buffer;
        context->bufferlen = buffer_size;
        context->completionhandler = std::allocate_shared<RW_CompletionHandler>(Context_->RW_CompletionHandlerAllocator);
        context->completionhandler->completionhandler = std::move(handler);
        context->IOOperation = IO_OPERATION::IoRead;
        continue_io(true, context);
    }

    void Socket::continue_io(bool success, Win_IO_RW_Context *context)
    {
        if (!success) {
            close();
            context->completionhandler->handle(TranslateError(), 0, true);
            Context_->Win_IO_RW_ContextAllocator.deallocate(context, 1);
        }
        else if (context->bufferlen == context->transfered_bytes) {
            context->completionhandler->handle(StatusCode::SC_SUCCESS, context->transfered_bytes, true);
            Context_->Win_IO_RW_ContextAllocator.deallocate(context, 1);
        }
        else {
            auto func = context->completionhandler;
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
                Context_->Win_IO_RW_ContextAllocator.deallocate(context, 1);
            }
            else if (nRet == 0 && dwSendNumBytes == bytesleft) {
                func->handle(StatusCode::SC_SUCCESS, bytesleft, true);
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
        if (!success) {
            context->completionhandler(TranslateError());
            Context_->Win_IO_Connect_ContextAllocator.deallocate(context, 1);
        }
        handle = INTERNAL::Socket(context->address.get_Family());
        BindSocket(handle, context->address.get_Family());
        if (!setsockopt<SocketOptions::O_BLOCKING>(Blocking_Options::NON_BLOCKING) || !Socket::UpdateIOCP(handle, &Context_->iocp.handle, this)) {
            context->completionhandler(TranslateError());
            Context_->Win_IO_Connect_ContextAllocator.deallocate(context, 1);
        }
        context->IOOperation = IO_OPERATION::IoConnect;
        context->Overlapped = {0};
        Context_->PendingIO += 1;
        auto connectres = Context_->ConnectEx_(handle, (::sockaddr *)context->address.get_SocketAddr(), context->address.get_SocketAddrLen(), 0, 0, 0,
                                               (LPOVERLAPPED)&context->Overlapped);
        if (connectres == TRUE) {
            Context_->PendingIO -= 1;
            context->completionhandler(StatusCode::SC_SUCCESS);
            return Context_->Win_IO_Connect_ContextAllocator.deallocate(context, 1);
        }
        else if (auto err = WSAGetLastError(); !(connectres == FALSE && err == ERROR_IO_PENDING)) {
            Context_->PendingIO -= 1;
            context->completionhandler(TranslateError(&err));
            Context_->Win_IO_Connect_ContextAllocator.deallocate(context, 1);
        }
    }

    void Socket::continue_connect(bool success, Win_IO_Connect_Context *context)
    {
        if (success && ::setsockopt(handle, SOL_SOCKET, SO_UPDATE_CONNECT_CONTEXT, 0, 0) != SOCKET_ERROR) {
            context->completionhandler(StatusCode::SC_SUCCESS);
            return Context_->Win_IO_Connect_ContextAllocator.deallocate(context, 1);
        }
        context->completionhandler(TranslateError());
        Context_->Win_IO_Connect_ContextAllocator.deallocate(context, 1);
    }
    void Socket::connect(sockaddr &address, const std::function<void(StatusCode)> &&handler)
    {
        auto context = Context_->Win_IO_Connect_ContextAllocator.allocate(1);
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
                context->completionhandler(TranslateError());
                Context_->Win_IO_Connect_ContextAllocator.deallocate(context, 1);
            }
        }
    }
    void Socket::send(size_t buffer_size, unsigned char *buffer, std::function<void(StatusCode, size_t)> &&handler)
    {
        auto context = Context_->Win_IO_RW_ContextAllocator.allocate(1);
        context->buffer = buffer;
        context->bufferlen = buffer_size;
        context->completionhandler = std::allocate_shared<RW_CompletionHandler>(Context_->RW_CompletionHandlerAllocator);
        context->completionhandler->completionhandler = std::move(handler);
        context->IOOperation = IO_OPERATION::IoWrite;
        continue_io(true, context);
    }

} // namespace NET
} // namespace SL
