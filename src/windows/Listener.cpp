
#include "Context.h"
#include "Listener.h"
#include "Socket.h"
#include <assert.h>

namespace SL {
namespace NET {

    Listener::Listener(Context *context, std::shared_ptr<ISocket> &&socket, const sockaddr &addr)
        : Context_(context), ListenSocket(std::static_pointer_cast<Socket>(socket)), ListenSocketAddr(addr)
    {
    }
    Listener::~Listener() {}

    void Listener::close() { ListenSocket->close(); }
    void Listener::start_accept(bool success, Win_IO_Accept_Context *context)
    {
        if (!success) {
            context->completionhandler(TranslateError(), std::shared_ptr<Socket>());
            context->Context_->Win_IO_Accept_ContextAllocator.deallocate(context, 1);
        }
        DWORD recvbytes = 0;
        context->Socket_ = std::allocate_shared<Socket>(context->Context_->SocketAllocator, context->Context_, context->Family);
        context->IOOperation = IO_OPERATION::IoAccept;
        context->Overlapped = {0};

        context->Context_->PendingIO += 1;

        auto nRet = context->Context_->AcceptEx_(context->ListenSocket, context->Socket_->get_handle(), (LPVOID)(context->Buffer), 0,
                                                 sizeof(SOCKADDR_STORAGE) + 16, sizeof(SOCKADDR_STORAGE) + 16, &recvbytes,
                                                 (LPOVERLAPPED) & (context->Overlapped));
        if (nRet == TRUE) {
            context->Context_->PendingIO -= 1;
            handle_accept(true, context);
        }
        else if (auto err = WSAGetLastError(); !(nRet == FALSE && err == ERROR_IO_PENDING)) {
            context->Context_->PendingIO -= 1;
            context->completionhandler(TranslateError(&err), std::shared_ptr<Socket>());
            context->Context_->Win_IO_Accept_ContextAllocator.deallocate(context, 1);
        }
    }
    void Listener::handle_accept(bool success, Win_IO_Accept_Context *context)
    {
        if (!success ||
            ::setsockopt(context->Socket_->get_handle(), SOL_SOCKET, SO_UPDATE_ACCEPT_CONTEXT, (char *)&context->ListenSocket,
                         sizeof(context->ListenSocket)) == SOCKET_ERROR ||
            !context->Socket_->setsockopt<SocketOptions::O_BLOCKING>(Blocking_Options::NON_BLOCKING) ||
            !Socket::UpdateIOCP(context->Socket_->get_handle(), &context->Context_->iocp.handle, context->Socket_.get())) {
            context->completionhandler(TranslateError(), std::shared_ptr<Socket>());
        }
        else {
            context->completionhandler(StatusCode::SC_SUCCESS, context->Socket_);
        }
        context->Context_->Win_IO_Accept_ContextAllocator.deallocate(context, 1);
    }
    void Listener::accept(const std::function<void(StatusCode, const std::shared_ptr<ISocket> &)> &&handler)
    {
        auto context = Context_->Win_IO_Accept_ContextAllocator.allocate(1);
        context->completionhandler = std::move(handler);
        context->IOOperation = IO_OPERATION::IoStartAccept;
        context->Context_ = Context_;
        context->Family = ListenSocketAddr.get_Family();
        context->ListenSocket = ListenSocket->get_handle();
        if (Context_->inWorkerThread()) {
            start_accept(true, context);
        }
        else {
            Context_->PendingIO += 1;
            if (PostQueuedCompletionStatus(Context_->iocp.handle, 0, (ULONG_PTR)this, (LPOVERLAPPED)&context->Overlapped) == FALSE) {
                Context_->PendingIO -= 1;
                context->completionhandler(TranslateError(), std::shared_ptr<Socket>());
                Context_->Win_IO_Accept_ContextAllocator.deallocate(context, 1);
            }
        }
    }
} // namespace NET
} // namespace SL