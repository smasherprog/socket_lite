
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
        auto addr = listensocket->getpeername();
        if (!addr.has_value()) {
            return std::shared_ptr<IListener>();
        }
        auto listener = std::make_shared<Listener>(context->PendingIO, std::forward<std::shared_ptr<ISocket>>(listensocket), addr.value());

        if (!Socket::UpdateIOCP(listener->ListenSocket->get_handle(), &context->iocp.handle, listener->ListenSocket.get())) {
            return std::shared_ptr<IListener>();
        }

        return listener;
    }

    Listener::Listener(std::atomic<size_t> &context, std::shared_ptr<ISocket> &&socket, const sockaddr &addr)
        : PendingIO(context), ListenSocket(std::static_pointer_cast<Socket>(socket)), ListenSocketAddr(addr)
    {
    }
    Listener::~Listener() {}

    void Listener::close() { ListenSocket->close(); }

    void Listener::async_accept(const std::function<void(const std::shared_ptr<ISocket> &)> &&handler)
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
        Win_IO_Accept_Context_.Socket_ = std::make_shared<Socket>(PendingIO, ListenSocketAddr.get_Family());
        if (!Win_IO_Accept_Context_.Socket_->setsockopt<Socket_Options::O_BLOCKING>(Blocking_Options::NON_BLOCKING)) {
            std::cerr << "failed to set socket to non blocking " << WSAGetLastError() << std::endl;
            handler(false);
        }
        else {
            Win_IO_Accept_Context_.IOOperation = IO_OPERATION::IoAccept;
            Win_IO_Accept_Context_.ListenSocket = ListenSocket->get_handle();

            Win_IO_Accept_Context_.completionhandler = std::move(handler);
            PendingIO += 1;
            auto nRet = AcceptEx_(ListenSocket->get_handle(), Win_IO_Accept_Context_.Socket_->get_handle(), (LPVOID)(Buffer), 0,
                                  sizeof(SOCKADDR_STORAGE) + 16, sizeof(SOCKADDR_STORAGE) + 16, &recvbytes,
                                  (LPOVERLAPPED) & (Win_IO_Accept_Context_.Overlapped));

            if (auto wsaerr = WSAGetLastError(); nRet == SOCKET_ERROR && (ERROR_IO_PENDING != wsaerr)) {
                PendingIO -= 1;
                std::cerr << "Error AcceptEx_ Code: " << wsaerr << std::endl;
                auto handl(std::move(Win_IO_Accept_Context_.completionhandler));
                Win_IO_Accept_Context_.clear();
                handl(false);
            }
        }
    }
} // namespace NET
} // namespace SL