
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
    void Listener::handle_accept(bool success, Win_IO_Accept_Context *context)
    {
        if (!success ||
            ::setsockopt(context->Socket_->get_handle(), SOL_SOCKET, SO_UPDATE_ACCEPT_CONTEXT, (char *)&context->ListenSocket,
                         sizeof(context->ListenSocket)) == SOCKET_ERROR ||
            CreateIoCompletionPort((HANDLE)context->Socket_->get_handle(), context->Context_->IOCPHandle, NULL, NULL) == NULL) {
            if (auto h(std::move(context->completionhandler)); h) {
                context->reset();
                h(TranslateError(), std::shared_ptr<Socket>());
            }
        }
        else if (auto h(std::move(context->completionhandler)); h) {
            auto s(context->Socket_);
            context->reset();
            h(StatusCode::SC_SUCCESS, s);
        }
    }
    void Listener::start_accept(bool success, Win_IO_Accept_Context *context) {}
    void Listener::accept(const std::function<void(StatusCode, const std::shared_ptr<ISocket> &)> &&handler)
    {
        assert(Win_IO_Accept_Context_.IOOperation == IO_OPERATION::IoNone);
        Win_IO_Accept_Context_.Socket_ = std::make_shared<Socket>(Context_, ListenSocketAddr.get_Family());
        Win_IO_Accept_Context_.IOOperation = IO_OPERATION::IoAccept;
        Win_IO_Accept_Context_.completionhandler = std::move(handler);
        Win_IO_Accept_Context_.Context_ = Context_;
        Win_IO_Accept_Context_.ListenSocket = ListenSocket->get_handle();
        Context_->PendingIO += 1;
        DWORD recvbytes = 0;
        auto nRet = Context_->AcceptEx_(ListenSocket->get_handle(), Win_IO_Accept_Context_.Socket_->get_handle(),
                                        (LPVOID)(Win_IO_Accept_Context_.Buffer), 0, sizeof(SOCKADDR_STORAGE) + 16, sizeof(SOCKADDR_STORAGE) + 16,
                                        &recvbytes, (LPOVERLAPPED) & (Win_IO_Accept_Context_.Overlapped));
        if (nRet == TRUE) {
            Context_->PendingIO -= 1;
            handle_accept(true, &Win_IO_Accept_Context_);
        }
        else if (auto err = WSAGetLastError(); !(nRet == FALSE && err == ERROR_IO_PENDING)) {
            Context_->PendingIO -= 1;
            if (auto h(std::move(Win_IO_Accept_Context_.completionhandler)); h) {
                Win_IO_Accept_Context_.reset();
                h(TranslateError(&err), std::shared_ptr<Socket>());
            }
        }
    }
} // namespace NET
} // namespace SL