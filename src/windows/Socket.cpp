
#include "Socket_Lite.h"
#include <assert.h>

namespace SL {
namespace NET {

    bool Socket::UpdateIOCP(SOCKET socket, HANDLE *iocp)
    {
        if (*iocp = CreateIoCompletionPort((HANDLE)socket, *iocp, 0, 0); *iocp != NULL) {
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
        context->Socket_ = handle;
        context->IOOperation = IO_OPERATION::IoRead;
        continue_io(true, context, Context_);
    }

    void Socket::send(size_t buffer_size, unsigned char *buffer, std::function<void(StatusCode, size_t)> &&handler)
    {
        auto context = Context_->Win_IO_RW_ContextBuffer.newObject();
        context->buffer = buffer;
        context->bufferlen = buffer_size;
        context->completionhandler = Context_->RW_CompletionHandlerBuffer.newObject();
        context->completionhandler->completionhandler = std::move(handler);
        context->completionhandler->RefCount = 1;
        context->Socket_ = handle;
        context->IOOperation = IO_OPERATION::IoWrite;
        continue_io(true, context, Context_);
    }
    void Socket::continue_io(bool success, Win_IO_RW_Context *context, Context *iocontext)
    {
        if (!success) {
            context->completionhandler->handle(TranslateError(), 0, true);
            if (context->completionhandler->RefCount.fetch_sub(1, std::memory_order_relaxed) == 1) {
                iocontext->RW_CompletionHandlerBuffer.deleteObject(context->completionhandler);
            }
            iocontext->Win_IO_RW_ContextBuffer.deleteObject(context);
        }
        else if (context->bufferlen == context->transfered_bytes) {
            context->completionhandler->handle(StatusCode::SC_SUCCESS, context->transfered_bytes, true);
            if (context->completionhandler->RefCount.fetch_sub(1, std::memory_order_relaxed) == 1) {
                iocontext->RW_CompletionHandlerBuffer.deleteObject(context->completionhandler);
            }
            iocontext->Win_IO_RW_ContextBuffer.deleteObject(context);
        }
        else {
            auto func = context->completionhandler;
            func->RefCount.fetch_add(1, std::memory_order_relaxed);
            WSABUF wsabuf;
            auto bytesleft = static_cast<decltype(wsabuf.len)>(context->bufferlen - context->transfered_bytes);
            wsabuf.buf = (char *)context->buffer + context->transfered_bytes;
            wsabuf.len = bytesleft;
            DWORD dwSendNumBytes(0), dwFlags(0);
            iocontext->PendingIO += 1;
            DWORD nRet = 0;
            if (context->IOOperation == IO_OPERATION::IoRead) {
                nRet = WSARecv(context->Socket_, &wsabuf, 1, &dwSendNumBytes, &dwFlags, &(context->Overlapped), NULL);
            }
            else {
                nRet = WSASend(context->Socket_, &wsabuf, 1, &dwSendNumBytes, dwFlags, &(context->Overlapped), NULL);
            }
            auto lasterr = WSAGetLastError();
            if (nRet == SOCKET_ERROR && (WSA_IO_PENDING != lasterr)) {
                iocontext->PendingIO -= 1;
                closesocket(context->Socket_);
                func->handle(TranslateError(&lasterr), 0, false);
                iocontext->RW_CompletionHandlerBuffer.deleteObject(func);
                iocontext->Win_IO_RW_ContextBuffer.deleteObject(context);
            }
            else if (nRet == 0 && dwSendNumBytes == bytesleft) {
                func->handle(StatusCode::SC_SUCCESS, bytesleft, true);
                if (func->RefCount.fetch_sub(1, std::memory_order_relaxed) == 1) {
                    iocontext->RW_CompletionHandlerBuffer.deleteObject(func);
                }
            }
            else {
                if (func->RefCount.fetch_sub(1, std::memory_order_relaxed) == 1) {
                    iocontext->RW_CompletionHandlerBuffer.deleteObject(func);
                }
            }
        }
    }
    bool BindSocket(SOCKET &sock, AddressFamily family)
    {
        if (family == AddressFamily::IPV4) {
            sockaddr_in bindaddr = {0};
            bindaddr.sin_family = AF_INET;
            sockaddr mytestaddr((unsigned char *)&bindaddr, sizeof(bindaddr), "", 0, AddressFamily::IPV4);
            if (INTERNAL::bind(sock, mytestaddr) != StatusCode::SC_SUCCESS) {
                return false;
            }
        }
        else {
            sockaddr_in6 bindaddr = {0};
            bindaddr.sin6_family = AF_INET6;
            bindaddr.sin6_addr = in6addr_any;
            sockaddr mytestaddr((unsigned char *)&bindaddr, sizeof(bindaddr), "", 0, AddressFamily::IPV6);
            if (INTERNAL::bind(sock, mytestaddr) != StatusCode::SC_SUCCESS) {
                return false;
            }
        }
        return true;
    }
    void Socket::init_connect(bool success, Win_IO_Connect_Context *context, Context *iocontext)
    {
        if (!success) {
            closesocket(context->Socket_);
            context->completionhandler(TranslateError(), Socket());
            return iocontext->Win_IO_Connect_ContextBuffer.deleteObject(context);
        };
        SOCKET handle = INVALID_SOCKET;
        if (!BindSocket(handle, context->address.get_Family()) || !INTERNAL::setsockopt_O_BLOCKING(handle, Blocking_Options::NON_BLOCKING) ||
            !Socket::UpdateIOCP(handle, &iocontext->iocp.handle)) {
            context->completionhandler(TranslateError(), Socket());
            closesocket(handle);
            return iocontext->Win_IO_Connect_ContextBuffer.deleteObject(context);
        }
        context->Socket_ = handle;
        context->IOOperation = IO_OPERATION::IoConnect;
        context->Overlapped = {0};
        iocontext->PendingIO += 1;
        auto connectres = iocontext->ConnectEx_(handle, (::sockaddr *)context->address.get_SocketAddr(), context->address.get_SocketAddrLen(), 0, 0,
                                                0, (LPOVERLAPPED)&context->Overlapped);
        if (connectres == TRUE) {
            iocontext->PendingIO -= 1;
            Socket s(iocontext);
            s.set_handle(handle);
            context->completionhandler(StatusCode::SC_SUCCESS, std::move(s));
            iocontext->Win_IO_Connect_ContextBuffer.deleteObject(context);
        }
        else if (auto err = WSAGetLastError(); !(connectres == FALSE && err == ERROR_IO_PENDING)) {
            iocontext->PendingIO -= 1;
            closesocket(context->Socket_);
            context->completionhandler(TranslateError(&err), Socket());
            iocontext->Win_IO_Connect_ContextBuffer.deleteObject(context);
        }
    }

    void Socket::continue_connect(bool success, Win_IO_Connect_Context *context, Context *iocontext)
    {
        if (success) {
            if (::setsockopt(context->Socket_, SOL_SOCKET, SO_UPDATE_CONNECT_CONTEXT, 0, 0) != SOCKET_ERROR) {
                Socket s(iocontext);
                s.set_handle(context->Socket_);
                context->completionhandler(StatusCode::SC_SUCCESS, std::move(s));
                return iocontext->Win_IO_Connect_ContextBuffer.deleteObject(context);
            }
        }
        closesocket(context->Socket_);
        context->completionhandler(TranslateError(), Socket());
        iocontext->Win_IO_Connect_ContextBuffer.deleteObject(context);
    }
    void Socket::connect(Context *iocontext, sockaddr &address, const std::function<void(StatusCode, Socket &&)> &&handler)
    {
        auto context = iocontext->Win_IO_Connect_ContextBuffer.newObject();
        context->completionhandler = std::move(handler);
        context->IOOperation = IO_OPERATION::IoInitConnect;
        context->address = address;
        if (iocontext->inWorkerThread()) {
            init_connect(true, context, iocontext);
        }
        else {
            iocontext->PendingIO += 1;
            if (PostQueuedCompletionStatus(iocontext->iocp.handle, 0, 0, (LPOVERLAPPED)&context->Overlapped) == FALSE) {
                iocontext->PendingIO -= 1;
                context->completionhandler(TranslateError(), Socket());
                iocontext->Win_IO_Connect_ContextBuffer.deleteObject(context);
            }
        }
    }

} // namespace NET
} // namespace SL
