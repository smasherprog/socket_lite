
#include "Context.h"
#include "Listener.h"
#include "Socket.h"
#include <assert.h>

namespace SL {
namespace NET {

    Listener::Listener(Context *context, std::shared_ptr<ISocket> &&socket, const sockaddr &addr)
        : Context_(context), ListenSocket(std::static_pointer_cast<Socket>(socket)), ListenSocketAddr(addr)
    {
        if (!AcceptEx_) {
            GUID acceptex_guid = WSAID_ACCEPTEX;
            DWORD bytes = 0;
            WSAIoctl(ListenSocket->get_handle(), SIO_GET_EXTENSION_FUNCTION_POINTER, &acceptex_guid, sizeof(acceptex_guid), &AcceptEx_,
                     sizeof(AcceptEx_), &bytes, NULL, NULL);
        }
    }
    Listener::~Listener() {}

    void Listener::close() { ListenSocket->close(); }
    void Listener::start_accept(bool success, Win_IO_Accept_Context *context)
    {
        static auto iofailed = [](auto code, auto context) {
            auto chandle(std::move(context->completionhandler));
            delete context;
            chandle(code, std::shared_ptr<Socket>());
        };
        if (!success) {
            return iofailed(TranslateError(), context);
        }
        DWORD recvbytes = 0;
        context->Socket_ = std::make_shared<Socket>(Context_, ListenSocketAddr.get_Family());
        context->IOOperation = IO_OPERATION::IoAccept;
        context->Overlapped = {0};
        context->ListenSocket = ListenSocket->get_handle();
        Context_->PendingIO += 1;

        auto nRet = AcceptEx_(ListenSocket->get_handle(), context->Socket_->get_handle(), (LPVOID)(Buffer), 0, sizeof(SOCKADDR_STORAGE) + 16,
                              sizeof(SOCKADDR_STORAGE) + 16, &recvbytes, (LPOVERLAPPED) & (context->Overlapped));
        if (nRet == TRUE) {
            Context_->PendingIO -= 1;
            handle_accept(true, context);
        }
        else if (auto err = WSAGetLastError(); !(nRet == FALSE && err == ERROR_IO_PENDING)) {
            Context_->PendingIO -= 1;
            iofailed(TranslateError(&err), context);
        }
    }
    void Listener::handle_accept(bool success, Win_IO_Accept_Context *context)
    {
        static auto iodone = [](auto code, auto context) {
            auto sock(std::move(context->Socket_));
            auto chandle(std::move(context->completionhandler));
            delete context;
            chandle(code, sock);
        };

        if (::setsockopt(context->Socket_->get_handle(), SOL_SOCKET, SO_UPDATE_ACCEPT_CONTEXT, (char *)&context->ListenSocket,
                         sizeof(context->ListenSocket)) == SOCKET_ERROR) {
            return iodone(TranslateError(), context);
        }
        context->Socket_->setsockopt<SocketOptions::O_BLOCKING>(Blocking_Options::NON_BLOCKING);
        if (!success || (success && !Socket::UpdateIOCP(context->Socket_->get_handle(), &Context_->iocp.handle, context->Socket_.get()))) {
            iodone(TranslateError(), context);
        }
        else {
            iodone(StatusCode::SC_SUCCESS, context);
        }
    }
    void Listener::accept(const std::function<void(StatusCode, const std::shared_ptr<ISocket> &)> &&handler)
    {
        static auto iofailed = [](auto code, auto context) {
            auto chandle(std::move(context->completionhandler));
            delete context;
            chandle(code, std::shared_ptr<Socket>());
        };
        auto context = new Win_IO_Accept_Context();
        context->completionhandler = std::move(handler);
        context->IOOperation = IO_OPERATION::IoStartAccept;
        if (Context_->inWorkerThread()) {
            start_accept(true, context);
        }
        else {
            Context_->PendingIO += 1;
            if (PostQueuedCompletionStatus(Context_->iocp.handle, 0, (ULONG_PTR)this, (LPOVERLAPPED)&context->Overlapped) == FALSE) {
                Context_->PendingIO -= 1;
                iofailed(TranslateError(), context);
            }
        }
    }
} // namespace NET
} // namespace SL