#include "Acceptor.h"

namespace SL {
namespace NET {

    Acceptor::Acceptor(Context &context, unsigned short port) : Context_(context)
    {
        if (!context)
            return;
        LINGER lingerStruct;
        struct addrinfo hints = {0};
        struct addrinfo *addrlocal = NULL;

        lingerStruct.l_onoff = 1;
        lingerStruct.l_linger = 0;

        hints.ai_flags = AI_PASSIVE;
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_protocol = IPPROTO_IP;
        auto strport = std::to_string(port);
        if (auto ret = getaddrinfo(NULL, strport.c_str(), &hints, &addrlocal); ret != 0 || addrlocal == NULL) {
            SocketContext_.close();
            return;
        }
        if (auto nRet = bind(SocketContext_.handle, addrlocal->ai_addr, (int)addrlocal->ai_addrlen); nRet == SOCKET_ERROR) {
            SocketContext_.close();
            freeaddrinfo(addrlocal);
            return;
        }
        freeaddrinfo(addrlocal);
        if (auto nRet = listen(SocketContext_.handle, 5); nRet == SOCKET_ERROR) {
            SocketContext_.close();
            return;
        }

        GUID acceptex_guid = WSAID_ACCEPTEX;
        DWORD bytes = 0;
        if (auto nRet = WSAIoctl(SocketContext_.handle, SIO_GET_EXTENSION_FUNCTION_POINTER, &acceptex_guid, sizeof(acceptex_guid), &AcceptEx_,
                                 sizeof(AcceptEx_), &bytes, NULL, NULL);
            nRet == SOCKET_ERROR) {
            SocketContext_.close();
        }
        if (auto ctx = CreateIoCompletionPort((HANDLE)SocketContext_.handle, Context_.iocp.handle, (DWORD_PTR)&SocketContext_, 0); ctx == NULL) {
            SocketContext_.close();
        }
        else {
            Context_.iocp.handle = ctx;
        }
    }
    Acceptor::~Acceptor() { close(); }
    void Acceptor::close()
    {
        SocketContext_.close();
        while (!HasOverlappedIoCompleted((LPOVERLAPPED)&SocketContext_.Overlapped))
            Sleep(0);
    }
    void Acceptor::async_accept(const std::shared_ptr<Socket> &socket)
    {
        DWORD recvbytes = 0;

        auto nRet = AcceptEx_(SocketContext_.handle, socket->handle, (LPVOID)(AcceptBuffer), 0, sizeof(SOCKADDR_STORAGE) + 16,
                              sizeof(SOCKADDR_STORAGE) + 16, &recvbytes, (LPOVERLAPPED) & (SocketContext_.Overlapped));
        if (nRet == SOCKET_ERROR && (ERROR_IO_PENDING != WSAGetLastError())) {
        }
    }
} // namespace NET
} // namespace SL