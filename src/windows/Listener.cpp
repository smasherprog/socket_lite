
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

        auto iocontext = new Win_IO_Accept_Context();
        iocontext->IOOperation = IO_OPERATION::IoAccept;
        iocontext->ListenSocket = ListenSocket->get_handle();
        iocontext->Socket_ = std::static_pointer_cast<Socket>(socket);
        iocontext->completionhandler = std::move(handler);

        PendingIO += 1;
        auto nRet = AcceptEx_(ListenSocket->get_handle(), iocontext->Socket_->get_handle(), (LPVOID)(Buffer),
                              MAX_BUFF_SIZE - (2 * (sizeof(SOCKADDR_STORAGE) + 16)), sizeof(SOCKADDR_STORAGE) + 16, sizeof(SOCKADDR_STORAGE) + 16,
                              &recvbytes, (LPOVERLAPPED) & (iocontext->Overlapped));

        if (auto wsaerr = WSAGetLastError(); nRet == SOCKET_ERROR && (ERROR_IO_PENDING != wsaerr)) {
            std::cerr << "Error AcceptEx_ Code: " << wsaerr << std::endl;
            PendingIO -= 1;
            iocontext->completionhandler(false);
            delete iocontext;
        }
    }

} // namespace NET
} // namespace SL