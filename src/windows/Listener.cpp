
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
        auto &iocontext = *context->Context_;
        if (!success) {
            context->completionhandler(TranslateError(), std::shared_ptr<Socket>());
            delete context;
            return;
        }
        DWORD recvbytes = 0;
        context->Socket_ = std::make_shared<Socket>(context->Context_, context->Family);
        context->IOOperation = IO_OPERATION::IoAccept;
        context->Overlapped = {0};
        iocontext.PendingIO += 1;
        auto nRet =
            iocontext.AcceptEx_(context->ListenSocket, context->Socket_->get_handle(), (LPVOID)(context->Buffer), 0, sizeof(SOCKADDR_STORAGE) + 16,
                                sizeof(SOCKADDR_STORAGE) + 16, &recvbytes, (LPOVERLAPPED) & (context->Overlapped));
        if (nRet == TRUE) {
            iocontext.PendingIO -= 1;
            handle_accept(true, context);
        }
        else if (auto err = WSAGetLastError(); !(nRet == FALSE && err == ERROR_IO_PENDING)) {
            iocontext.PendingIO -= 1;
            context->completionhandler(TranslateError(&err), std::shared_ptr<Socket>());
            delete context;
            return;
        }
    }
    void Listener::handle_accept(bool success, Win_IO_Accept_Context *context)
    {
        if (!success ||
            ::setsockopt(context->Socket_->get_handle(), SOL_SOCKET, SO_UPDATE_ACCEPT_CONTEXT, (char *)&context->ListenSocket,
                         sizeof(context->ListenSocket)) == SOCKET_ERROR ||
            CreateIoCompletionPort((HANDLE)context->Socket_->get_handle(), context->Context_->IOCPHandle, NULL, NULL) == NULL) {
            context->completionhandler(TranslateError(), std::shared_ptr<Socket>());
        }
        else {
            context->completionhandler(StatusCode::SC_SUCCESS, context->Socket_);
        }
        delete context;
    }
    void Listener::accept(const std::function<void(StatusCode, const std::shared_ptr<ISocket> &)> &&handler)
    {
        auto context = new Win_IO_Accept_Context();
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
            if (PostQueuedCompletionStatus(Context_->IOCPHandle, 0, (ULONG_PTR)this, (LPOVERLAPPED)&context->Overlapped) == FALSE) {
                Context_->PendingIO -= 1;
                context->completionhandler(TranslateError(), std::shared_ptr<Socket>());
                delete context;
            }
        }
    }
} // namespace NET
} // namespace SL