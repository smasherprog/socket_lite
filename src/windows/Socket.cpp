
#include "Context.h"
#include "Socket.h"
#include <Ws2ipdef.h>
#include <assert.h>
namespace SL {
namespace NET {

    Socket::Socket(Context *context, AddressFamily family) : Socket(context) { handle = INTERNAL::Socket(family); }
    Socket::Socket(Context *context) : Context_(context) {}
    Socket::~Socket() {}

    void Socket::recv(size_t buffer_size, unsigned char *buffer, std::function<void(StatusCode, size_t)> &&handler)
    {
        assert(ReadContext.IOOperation == IO_OPERATION::IoNone);
        ReadContext.buffer = buffer;
        ReadContext.bufferlen = buffer_size;
        ReadContext.Context_ = Context_;
        ReadContext.Socket_ = this;
        ReadContext.completionhandler = std::move(handler);
        ReadContext.Completed = 1;
        ReadContext.IOOperation = IO_OPERATION::IoRead;
        continue_io(true, ReadContext);
    }
    void Socket::send(size_t buffer_size, unsigned char *buffer, std::function<void(StatusCode, size_t)> &&handler)
    {
        assert(WriteContext.IOOperation == IO_OPERATION::IoNone);
        WriteContext.buffer = buffer;
        WriteContext.bufferlen = buffer_size;
        WriteContext.Context_ = Context_;
        WriteContext.Socket_ = this;
        WriteContext.completionhandler = std::move(handler);
        WriteContext.Completed = 1;
        WriteContext.IOOperation = IO_OPERATION::IoWrite;
        continue_io(true, WriteContext);
    }
    void Socket::continue_io(bool success, Win_IO_RW_Context &context)
    {

        if (!success) {
            if (context.Completed.fetch_sub(1, std::memory_order_relaxed) == 1) {
                context.Socket_->close();
                auto handler(std::move(context.completionhandler));
                context.reset();
                handler(TranslateError(), 0);
            }
        }
        else if (context.bufferlen == context.transfered_bytes) {
            if (context.Completed.fetch_sub(1, std::memory_order_relaxed) == 1) {
                auto handler(std::move(context.completionhandler));
                context.reset();
                handler(StatusCode::SC_SUCCESS, context.transfered_bytes);
            }
        }
        else {
            context.Completed.fetch_add(1, std::memory_order_relaxed);
            WSABUF wsabuf;
            auto bytesleft = static_cast<decltype(wsabuf.len)>(context.bufferlen - context.transfered_bytes);
            wsabuf.buf = (char *)context.buffer + context.transfered_bytes;
            wsabuf.len = bytesleft;
            DWORD dwSendNumBytes(0), dwFlags(0);
            context.Context_->PendingIO += 1;
            DWORD nRet = 0;
            if (context.IOOperation == IO_OPERATION::IoRead) {
                nRet = WSARecv(context.Socket_->get_handle(), &wsabuf, 1, &dwSendNumBytes, &dwFlags, &(context.Overlapped), NULL);
            }
            else {
                nRet = WSASend(context.Socket_->get_handle(), &wsabuf, 1, &dwSendNumBytes, dwFlags, &(context.Overlapped), NULL);
            }
            auto lasterr = WSAGetLastError();
            if (nRet == SOCKET_ERROR && (WSA_IO_PENDING != lasterr)) {
                context.Context_->PendingIO -= 1;
                if (context.Completed.fetch_sub(1, std::memory_order_relaxed) == 1) {
                    context.Socket_->close();
                    auto handler(std::move(context.completionhandler));
                    context.reset();
                    handler(TranslateError(&lasterr), 0);
                }
            }
            else if (nRet == 0 && dwSendNumBytes == bytesleft) {
                if (context.Completed.fetch_sub(1, std::memory_order_relaxed) == 1) {
                    auto handler(std::move(context.completionhandler));
                    context.reset();
                    handler(StatusCode::SC_SUCCESS, bytesleft);
                }
            }
            else {
                // if this succeeds then it means another thread has completed the process
                if (context.Completed.fetch_sub(1, std::memory_order_relaxed) == 1) {
                    auto handler(std::move(context.completionhandler));
                    if (context.bufferlen == context.transfered_bytes) {
                        context.reset();
                        handler(StatusCode::SC_SUCCESS, context.transfered_bytes);
                    }
                    else {
                        context.Socket_->close();
                        context.reset();
                        handler(TranslateError(), 0);
                    }
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
        auto &iocontext = *context->Context_;
        if (!success) {
            context->completionhandler(TranslateError());
            return iocontext.Win_IO_Connect_ContextAllocator.deallocate(context, 1);
        }

        auto handle = INTERNAL::Socket(context->address.get_Family());
        BindSocket(handle, context->address.get_Family());
        if (!INTERNAL::setsockopt_O_BLOCKING(handle, Blocking_Options::NON_BLOCKING) ||
            CreateIoCompletionPort((HANDLE)handle, iocontext.IOCPHandle, NULL, NULL) == NULL) {
            context->completionhandler(TranslateError());
            return iocontext.Win_IO_Connect_ContextAllocator.deallocate(context, 1);
        }
        context->IOOperation = IO_OPERATION::IoConnect;
        context->Overlapped = {0};
        iocontext.PendingIO += 1;
        context->Socket_->set_handle(handle);
        auto connectres = iocontext.ConnectEx_(handle, (::sockaddr *)context->address.get_SocketAddr(), context->address.get_SocketAddrLen(), 0, 0, 0,
                                               (LPOVERLAPPED)&context->Overlapped);
        if (connectres == TRUE) {
            iocontext.PendingIO -= 1;
            context->completionhandler(StatusCode::SC_SUCCESS);
            return iocontext.Win_IO_Connect_ContextAllocator.deallocate(context, 1);
        }
        else if (auto err = WSAGetLastError(); !(connectres == FALSE && err == ERROR_IO_PENDING)) {
            iocontext.PendingIO -= 1;
            context->completionhandler(TranslateError(&err));
            iocontext.Win_IO_Connect_ContextAllocator.deallocate(context, 1);
        }
    }

    void Socket::continue_connect(bool success, Win_IO_Connect_Context *context)
    {
        if (success && ::setsockopt(context->Socket_->get_handle(), SOL_SOCKET, SO_UPDATE_CONNECT_CONTEXT, 0, 0) != SOCKET_ERROR) {
            context->completionhandler(StatusCode::SC_SUCCESS);
            return context->Context_->Win_IO_Connect_ContextAllocator.deallocate(context, 1);
        }
        context->completionhandler(TranslateError());
        context->Context_->Win_IO_Connect_ContextAllocator.deallocate(context, 1);
    }
    void Socket::connect(sockaddr &address, const std::function<void(StatusCode)> &&handler)
    {
        auto context = Context_->Win_IO_Connect_ContextAllocator.allocate(1);
        context->completionhandler = std::move(handler);
        context->IOOperation = IO_OPERATION::IoInitConnect;
        context->Socket_ = this;
        context->address = address;
        context->Context_ = Context_;
        if (Context_->inWorkerThread()) {
            init_connect(true, context);
        }
        else {
            Context_->PendingIO += 1;
            if (PostQueuedCompletionStatus(Context_->IOCPHandle, 0, NULL, (LPOVERLAPPED)&context->Overlapped) == FALSE) {
                Context_->PendingIO -= 1;
                context->completionhandler(TranslateError());
                Context_->Win_IO_Connect_ContextAllocator.deallocate(context, 1);
            }
        }
    }

} // namespace NET
} // namespace SL
