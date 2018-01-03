#include "Acceptor.h"
#include "ListenContext.h"
#include "Socket.h"
#include <string>

namespace SL {
namespace NET {

    Acceptor::Acceptor(ListenContext &context, PortNumber port, NetworkProtocol protocol) : Context_(context), Protocol(protocol)
    {
        AcceptSocket = Socket::Create(Protocol);
        if (Protocol == NetworkProtocol::IPV4) {
            sockaddr_in serveraddr;
            memset(&serveraddr, 0, sizeof(serveraddr));
            serveraddr.sin_family = AF_INET;
            serveraddr.sin_port = htons(port.value);
            serveraddr.sin_addr.s_addr = INADDR_ANY;
            if (auto nRet = bind(AcceptSocket, (struct sockaddr *)&serveraddr, sizeof(serveraddr)); nRet == SOCKET_ERROR) {
                closesocket(AcceptSocket);
                return;
            }
        }
        else {
            sockaddr_in6 serveraddr;
            memset(&serveraddr, 0, sizeof(serveraddr));
            serveraddr.sin6_family = AF_INET6;
            serveraddr.sin6_port = htons(port.value);
            serveraddr.sin6_addr = in6addr_any;
            if (auto nRet = bind(AcceptSocket, (struct sockaddr *)&serveraddr, sizeof(serveraddr)); nRet == SOCKET_ERROR) {
                closesocket(AcceptSocket);
                return;
            }
        }
        if (auto nRet = listen(AcceptSocket, 5); nRet == SOCKET_ERROR) {
            closesocket(AcceptSocket);
            return;
        }

        GUID acceptex_guid = WSAID_ACCEPTEX;
        DWORD bytes = 0;
        if (auto nRet = WSAIoctl(AcceptSocket, SIO_GET_EXTENSION_FUNCTION_POINTER, &acceptex_guid, sizeof(acceptex_guid), &AcceptEx_,
                                 sizeof(AcceptEx_), &bytes, NULL, NULL);
            nRet == SOCKET_ERROR) {
            closesocket(AcceptSocket);
        }
        if (auto ctx = CreateIoCompletionPort((HANDLE)AcceptSocket, Context_.iocp.handle, NULL, 0); ctx == NULL) {
            closesocket(AcceptSocket);
        }
        else {
            Context_.iocp.handle = ctx;
        }
    }
    Acceptor::~Acceptor()
    {
        closesocket(AcceptSocket);
        if (LastContext) {
            while (!HasOverlappedIoCompleted((LPOVERLAPPED)&LastContext->Overlapped)) {
                Sleep(0);
            }
            freecontext(&LastContext);
        }
    }

    void Acceptor::async_accept()
    {
        DWORD recvbytes = 0;
        freecontext(&LastContext);
        auto sockcontext = createcontext(Protocol);
        auto nRet = AcceptEx_(AcceptSocket, sockcontext->Socket->handle, (LPVOID)(AcceptBuffer), 0, sizeof(SOCKADDR_STORAGE) + 16,
                              sizeof(SOCKADDR_STORAGE) + 16, &recvbytes, (LPOVERLAPPED) & (sockcontext->Overlapped));
        if (nRet == SOCKET_ERROR && (ERROR_IO_PENDING != WSAGetLastError())) {
        }
    }
} // namespace NET
} // namespace SL