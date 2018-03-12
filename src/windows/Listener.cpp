
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

    void Listener::async_accept(const std::function<void(StatusCode, const std::shared_ptr<ISocket> &)> &&handler)
    {
        DWORD recvbytes = 0;
        assert(Win_IO_Accept_Context_.IOOperation == IO_OPERATION::IoNone);
        Win_IO_Accept_Context_.clear();
        Win_IO_Accept_Context_.Socket_ = std::make_shared<Socket>(Context_, ListenSocketAddr.get_Family());
        if (!Win_IO_Accept_Context_.Socket_->setsockopt<SocketOptions::O_BLOCKING>(Blocking_Options::NON_BLOCKING)) {
            handler(TranslateError(), std::shared_ptr<ISocket>());
        }
        else {
            Win_IO_Accept_Context_.IOOperation = IO_OPERATION::IoAccept;
            Win_IO_Accept_Context_.ListenSocket = ListenSocket->get_handle();

            Win_IO_Accept_Context_.completionhandler = std::move(handler);
            Context_->PendingIO += 1;
            auto nRet = AcceptEx_(ListenSocket->get_handle(), Win_IO_Accept_Context_.Socket_->get_handle(), (LPVOID)(Buffer), 0,
                                  sizeof(SOCKADDR_STORAGE) + 16, sizeof(SOCKADDR_STORAGE) + 16, &recvbytes,
                                  (LPOVERLAPPED) & (Win_IO_Accept_Context_.Overlapped));

            if (auto wsaerr = WSAGetLastError(); nRet == SOCKET_ERROR && (ERROR_IO_PENDING != wsaerr)) {
                Context_->PendingIO -= 1;

                auto handl(std::move(Win_IO_Accept_Context_.completionhandler));
                Win_IO_Accept_Context_.clear();
                handl(TranslateError(&wsaerr), std::shared_ptr<ISocket>());
            }
        }
    }
} // namespace NET
} // namespace SL