#include "Acceptor.h"
#include "Socket.h"
#include <string>

namespace SL {
namespace NET {

    Acceptor::Acceptor(HANDLE *iocphandle, PortNumber port, NetworkProtocol protocol) : IOCPHandle(iocphandle), Protocol(protocol)
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
        if (!updateIOCP(AcceptSocket, IOCPHandle)) {
            closesocket(AcceptSocket);
        }
    }
    Acceptor::~Acceptor() { closesocket(AcceptSocket); }

    void Acceptor::async_accept()
    {
        auto socket = std::make_shared<Socket>(Protocol);
        socket->SendBuffers.push_back(PER_IO_CONTEXT());
        auto val = &socket->SendBuffers.front();
        val->Socket_ = socket;
        DWORD recvbytes = 0;
        auto nRet = AcceptEx_(AcceptSocket, socket->handle, (LPVOID)(AcceptBuffer), 0, sizeof(SOCKADDR_STORAGE) + 16, sizeof(SOCKADDR_STORAGE) + 16,
                              &recvbytes, (LPOVERLAPPED) & (val->Overlapped));
        if (nRet == SOCKET_ERROR && (ERROR_IO_PENDING != WSAGetLastError())) {
        }
    }
} // namespace NET
} // namespace SL