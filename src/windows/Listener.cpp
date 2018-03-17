
#include "Listener.h"
#include <assert.h>

namespace SL {
namespace NET {

    Listener::Listener(Context *context, std::shared_ptr<Socket> &&socket, const sockaddr &addr)
        : Context_(context), ListenSocket(std::static_pointer_cast<Socket>(socket)), ListenSocketAddr(addr)
    {
    }
    Listener::~Listener() { close(); }
    void Listener::close() { ListenSocket->close(); }
    void Listener::start_accept(bool success, Win_IO_Accept_Context *context, Context *iocontext)
    {
        if (!success) {
            context->completionhandler(TranslateError(), Socket());
            closesocket(context->Socket_);
            iocontext->Win_IO_Accept_ContextBuffer.deleteObject(context);
        }
        DWORD recvbytes = 0;
        context->Socket_ = INTERNAL::Socket(context->Family);
        context->IOOperation = IO_OPERATION::IoAccept;
        iocontext->PendingIO += 1;

        auto nRet = iocontext->AcceptEx_(context->ListenSocket, context->Socket_, (LPVOID)(context->Buffer), 0, sizeof(SOCKADDR_STORAGE) + 16,
                                         sizeof(SOCKADDR_STORAGE) + 16, &recvbytes, (LPOVERLAPPED) & (context->Overlapped));
        if (nRet == TRUE) {
            iocontext->PendingIO -= 1;
            handle_accept(true, context, iocontext);
        }
        else if (auto err = WSAGetLastError(); !(nRet == FALSE && err == ERROR_IO_PENDING)) {
            iocontext->PendingIO -= 1;
            context->completionhandler(TranslateError(&err), Socket());
            closesocket(context->Socket_);
            iocontext->Win_IO_Accept_ContextBuffer.deleteObject(context);
        }
    }
    void Listener::handle_accept(bool success, Win_IO_Accept_Context *context, Context *iocontext)
    {
        if (!success ||
            ::setsockopt(context->Socket_, SOL_SOCKET, SO_UPDATE_ACCEPT_CONTEXT, (char *)&context->ListenSocket, sizeof(context->ListenSocket)) ==
                SOCKET_ERROR ||
            !INTERNAL::setsockopt_O_BLOCKING(context->Socket_, Blocking_Options::NON_BLOCKING) ||
            !Socket::UpdateIOCP(context->Socket_, &iocontext->iocp.handle)) {
            closesocket(context->Socket_);
            context->completionhandler(TranslateError(), Socket());
            iocontext->Win_IO_Accept_ContextBuffer.deleteObject(context);
        }
        else {
            Socket s(iocontext);
            s.set_handle(context->Socket_);
            context->completionhandler(StatusCode::SC_SUCCESS, std::move(s));
            iocontext->Win_IO_Accept_ContextBuffer.deleteObject(context);
        }
    }
    void Listener::accept(const std::function<void(StatusCode, Socket &&)> &&handler)
    {
        auto context = Context_->Win_IO_Accept_ContextBuffer.newObject();
        context->completionhandler = std::move(handler);
        context->IOOperation = IO_OPERATION::IoStartAccept;
        context->Family = ListenSocketAddr.get_Family();
        context->ListenSocket = ListenSocket->get_handle();
        if (Context_->inWorkerThread()) {
            start_accept(true, context, Context_);
        }
        else {
            Context_->PendingIO += 1;
            if (PostQueuedCompletionStatus(Context_->iocp.handle, 0, (ULONG_PTR)this, (LPOVERLAPPED)&context->Overlapped) == FALSE) {
                Context_->PendingIO -= 1;
                context->completionhandler(TranslateError(), Socket());
                Context_->Win_IO_Accept_ContextBuffer.deleteObject(context);
            }
        }
    }
} // namespace NET
} // namespace SL