
#include "Context.h"
#include "Listener.h"
#include "Socket.h"
#include <assert.h>

namespace SL {
namespace NET {

    Listener::Listener(Context *context, std::shared_ptr<ISocket> &&socket, const sockaddr &addr)
        : PendingIO(context->PendingIO), iocp(context->iocp), ListenSocket(std::static_pointer_cast<Socket>(socket)), ListenSocketAddr(addr)
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
        context->Socket_ = std::make_shared<Socket>(PendingIO, iocp, ListenSocketAddr.get_Family());
        context->IOOperation = IO_OPERATION::IoAccept;
        context->ListenSocket = ListenSocket->get_handle();
        PendingIO += 1;
        auto nRet = AcceptEx_(ListenSocket->get_handle(), context->Socket_->get_handle(), (LPVOID)(Buffer), 0, sizeof(SOCKADDR_STORAGE) + 16,
                              sizeof(SOCKADDR_STORAGE) + 16, &recvbytes, (LPOVERLAPPED) & (context->Overlapped));
        if (auto wsaerr = WSAGetLastError(); nRet == SOCKET_ERROR && (ERROR_IO_PENDING != wsaerr)) {
            PendingIO -= 1;
            iofailed(TranslateError(&wsaerr), context);
        }
    }
    void Listener::handle_accept(bool success, Win_IO_Accept_Context *context)
    {
        static auto iodone = [](auto code, auto context, auto sock) {
            auto chandle(std::move(context->completionhandler));
            delete context;
            chandle(code, sock);
        };
        if (!success || (success && !context->Socket_->setsockopt<SocketOptions::O_BLOCKING>(Blocking_Options::NON_BLOCKING) &&
                         !Socket::UpdateIOCP(context->Socket_->get_handle(), &iocp.handle, context->Socket_.get()))) {
            iodone(TranslateError(), context, std::shared_ptr<Socket>());
        }
        else {
            iodone(StatusCode::SC_SUCCESS, context, std::move(context->Socket_));
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
        PendingIO += 1;
        if (PostQueuedCompletionStatus(iocp.handle, 0, (ULONG_PTR)this, (LPOVERLAPPED)&context->Overlapped) == FALSE) {
            PendingIO -= 1;
            iofailed(TranslateError(), context);
        }
    }
} // namespace NET
} // namespace SL