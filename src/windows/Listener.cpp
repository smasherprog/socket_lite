
#include "Socket_Lite.h"
#include "defs.h"
#include <assert.h>

namespace SL {
namespace NET {

    Listener::Listener(Context &context, PortNumber port, AddressFamily family, SL::NET::StatusCode &ec)
        : Context_(context), Family(family), ListenSocket(context)
    {
        ec = SL::NET::StatusCode::SC_CLOSED;
        auto[code, addresses] = SL::NET::getaddrinfo(nullptr, port, family);
        if (code != SL::NET::StatusCode::SC_SUCCESS) {
            ec = code;
        }
        else {
            for (auto &address : addresses) {
                SocketGetter sg(ListenSocket);
                if (ec = sg.bind(address); ec == SL::NET::StatusCode::SC_SUCCESS) {
                    if (ec = sg.listen(5); ec == SL::NET::StatusCode::SC_SUCCESS) {
                        SL::NET::setsockopt<SL::NET::SocketOptions::O_REUSEADDR>(ListenSocket, SL::NET::SockOptStatus::ENABLED);
                        if (auto e = CreateIoCompletionPort((HANDLE)sg.getSocket(), Context_.IOCPHandle, NULL, NULL); e == NULL) {
                            ListenSocket.close();
                            ec = TranslateError();
                        }
                    }
                }
            }
        }
    }
    Listener::~Listener() { ListenSocket.close(); }
    void handle_accept(bool success, Win_IO_Accept_Context *context, Socket &&socket)
    {
        auto h(std::move(context->completionhandler));
        SocketGetter sg(socket);
        if (!success ||
            ::setsockopt(context->Socket_, SOL_SOCKET, SO_UPDATE_ACCEPT_CONTEXT, (char *)&context->ListenSocket, sizeof(context->ListenSocket)) ==
                SOCKET_ERROR ||
            CreateIoCompletionPort((HANDLE)context->Socket_, sg.getIOCPHandle(), NULL, NULL) == NULL) {
            delete context;
            if (h) {
                socket.close();
                h(TranslateError(), std::move(socket));
            }
        }
        else {
            delete context;
            if (h) {
                h(StatusCode::SC_SUCCESS, std::move(socket));
            }
        }
    }

    void Listener::accept(const std::function<void(StatusCode, Socket)> &&handler)
    {
        if (!ListenSocket.isopen())
            return;
        SocketGetter sg(ListenSocket);
        auto context = new Win_IO_Accept_Context();
        context->Socket_ = INTERNAL::Socket(Family);
        context->IOOperation = INTERNAL::IO_OPERATION::IoAccept;
        context->completionhandler = std::move(handler);
        context->Context_ = &Context_;
        context->ListenSocket = sg.getSocket();
        Context_.PendingIO += 1;
        DWORD recvbytes = 0;
        auto nRet = Context_.AcceptEx_(sg.getSocket(), context->Socket_, (LPVOID)(context->Buffer), 0, sizeof(SOCKADDR_STORAGE) + 16,
                                       sizeof(SOCKADDR_STORAGE) + 16, &recvbytes, (LPOVERLAPPED) & (context->Overlapped));
        if (nRet == TRUE) {
            Context_.PendingIO -= 1;
            Socket sock(Context_);
            SocketGetter sg1(sock);
            sg1.setSocket(context->Socket_);
            handle_accept(true, context, std::move(sock));
        }
        else if (auto err = WSAGetLastError(); !(nRet == FALSE && err == ERROR_IO_PENDING)) {
            if (auto h(std::move(context->completionhandler)); h) {
                Context_.PendingIO -= 1;
                delete context;
                h(TranslateError(&err), Socket(Context_));
            }
        }
    }
} // namespace NET
} // namespace SL