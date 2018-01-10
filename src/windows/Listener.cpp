
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
        auto listener = std::make_shared<Listener>();
        auto context = static_cast<IO_Context *>(iocontext.get());
        listener->ListenSocket = std::static_pointer_cast<Socket>(listensocket);

        if (!Socket::UpdateIOCP(listener->ListenSocket->get_handle(), &context->iocp.handle, listener->ListenSocket.get())) {
            return std::shared_ptr<IListener>();
        }
        context->Listener_ = listener->ListenSocket; // make sure the iocontext has a reference to the listener
        return listener;
    }

    Listener::Listener(std::shared_ptr<Socket> &socket) : ListenSocket(socket) {}
    Listener::~Listener() {}

    void Listener::async_accept(std::shared_ptr<ISocket> &socket, const std::function<void(Bytes_Transfered)> &&handler)
    {
        if (!AcceptEx_) {
            GUID acceptex_guid = WSAID_ACCEPTEX;
            DWORD bytes = 0;
            if (WSAIoctl(ListenSocket->get_handle(), SIO_GET_EXTENSION_FUNCTION_POINTER, &acceptex_guid, sizeof(acceptex_guid), &AcceptEx_,
                         sizeof(AcceptEx_), &bytes, NULL, NULL) == SOCKET_ERROR) {
                std::cerr << "failed to load AcceptEx: " << WSAGetLastError() << std::endl;
                handler(-1);
            }
        }
        auto realsocket = static_cast<Socket *>(socket.get());
        DWORD recvbytes = 0;
        realsocket->ReadContext.clear();
        realsocket->ReadContext.IOOperation = IO_OPERATION::IoAccept;
        auto nRet = AcceptEx_(ListenSocket->get_handle(), realsocket->get_handle(), (LPVOID)(Buffer),
                              MAX_BUFF_SIZE - (2 * (sizeof(SOCKADDR_STORAGE) + 16)), sizeof(SOCKADDR_STORAGE) + 16, sizeof(SOCKADDR_STORAGE) + 16,
                              &recvbytes, (LPOVERLAPPED) & (realsocket->ReadContext.Overlapped));

        if (auto wsaerr = WSAGetLastError(); nRet == SOCKET_ERROR && (ERROR_IO_PENDING != wsaerr)) {
            std::cerr << "Error AcceptEx_ Code: " << wsaerr << std::endl;
        }
    }

} // namespace NET
} // namespace SL