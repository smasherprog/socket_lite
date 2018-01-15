
#include "IO_Context.h"
#include "Listener.h"
#include "Socket.h"
#include <assert.h>
#include <iostream>

namespace SL {
namespace NET {
    std::shared_ptr<IListener> SOCKET_LITE_EXTERN CreateListener(const std::shared_ptr<IIO_Context> &iocontext,
                                                                 std::shared_ptr<ISocket> &&listensocket)
    {
        auto context = static_cast<IO_Context *>(iocontext.get());
        auto listener = std::make_shared<Listener>(std::forward<std::shared_ptr<ISocket>>(listensocket), context->PendingIO);
        if (!Socket::UpdateIOCP(listener->ListenSocket->get_handle(), &context->iocp.handle, listener->ListenSocket.get())) {
            return std::shared_ptr<IListener>();
        }
        assert(!context->Listener_); // only one listner at a time please
        context->Listener_ = listener;
        return listener;
    }

    Listener::Listener(std::shared_ptr<ISocket> &&socket, std::atomic<size_t> &counter)
        : PendingIO(counter), ListenSocket(std::static_pointer_cast<Socket>(socket))
    {
    }
    Listener::~Listener() {}

    void Listener::async_accept(std::shared_ptr<ISocket> &socket, const std::function<void(bool)> &&handler)
    {
        if (!AcceptEx_) {
            GUID acceptex_guid = WSAID_ACCEPTEX;
            DWORD bytes = 0;
            if (WSAIoctl(ListenSocket->get_handle(), SIO_GET_EXTENSION_FUNCTION_POINTER, &acceptex_guid, sizeof(acceptex_guid), &AcceptEx_,
                         sizeof(AcceptEx_), &bytes, NULL, NULL) == SOCKET_ERROR) {
                std::cerr << "failed to load AcceptEx: " << WSAGetLastError() << std::endl;
                handler(false);
            }
        }

        DWORD recvbytes = 0;
        assert(Win_IO_Accept_Context_.IOOperation == IO_OPERATION::IoNone);

        Win_IO_Accept_Context_.IOOperation = IO_OPERATION::IoAccept;
        Win_IO_Accept_Context_.ListenSocket = ListenSocket->get_handle();
        Win_IO_Accept_Context_.Socket_ = std::static_pointer_cast<Socket>(socket);
        Win_IO_Accept_Context_.completionhandler = std::move(handler);

        PendingIO += 1;
        auto nRet =
            AcceptEx_(ListenSocket->get_handle(), Win_IO_Accept_Context_.Socket_->get_handle(), (LPVOID)(Buffer), 0, sizeof(SOCKADDR_STORAGE) + 16,
                      sizeof(SOCKADDR_STORAGE) + 16, &recvbytes, (LPOVERLAPPED) & (Win_IO_Accept_Context_.Overlapped));

        if (auto wsaerr = WSAGetLastError(); nRet == SOCKET_ERROR && (ERROR_IO_PENDING != wsaerr)) {
            std::cerr << "Error AcceptEx_ Code: " << wsaerr << std::endl;
            PendingIO -= 1;
            Win_IO_Accept_Context_.completionhandler(false);
            Win_IO_Accept_Context_.clear();
        }
    }
} // namespace NET
} // namespace SL