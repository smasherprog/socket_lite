#pragma once
#include "Context.h"
#include <string>
namespace SL {
namespace NET {
    class Acceptor {
        SOCKET socket = INVALID_SOCKET;
        LPFN_ACCEPTEX AcceptEx = nullptr;

      public:
        Acceptor(Context &context, unsigned short port)
        {

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
            if (getaddrinfo(NULL, strport.c_str(), &hints, &addrlocal) != 0) {
                return;
            }

            if (addrlocal == NULL) {
                return;
            }
            if (socket = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_IP, NULL, 0, WSA_FLAG_OVERLAPPED); socket == INVALID_SOCKET) {
                return;
            }
            auto nZero = 0;
            if (auto nRet = setsockopt(socket, SOL_SOCKET, SO_SNDBUF, (char *)&nZero, sizeof(nZero)); nRet == SOCKET_ERROR) {
                closesocket(socket);
                socket = INVALID_SOCKET;
                freeaddrinfo(addrlocal);
                return;
            }
            if (auto nRet = bind(socket, addrlocal->ai_addr, (int)addrlocal->ai_addrlen); nRet == SOCKET_ERROR) {
                closesocket(socket);
                socket = INVALID_SOCKET;
                freeaddrinfo(addrlocal);
                return;
            }

            if (auto nRet = listen(socket, 5); nRet == SOCKET_ERROR) {
                closesocket(socket);
                socket = INVALID_SOCKET;
            }

            freeaddrinfo(addrlocal);
            GUID acceptex_guid = WSAID_ACCEPTEX;
            DWORD bytes = 0;
            if (auto nRet = WSAIoctl(socket, SIO_GET_EXTENSION_FUNCTION_POINTER, &acceptex_guid, sizeof(acceptex_guid), &AcceptEx, sizeof(AcceptEx),
                                     &bytes, NULL, NULL);
                nRet == SOCKET_ERROR) {
                closesocket(socket);
                socket = INVALID_SOCKET;
            }
            SocketContext sockcontext;
            if (auto ctx = CreateIoCompletionPort((HANDLE)socket, context.iocp.handle, (DWORD_PTR)lpPerSocketContext, 0); ctx == NULL) {
                closesocket(socket);
                socket = INVALID_SOCKET;
            }
            else {
                context.iocp.handle = ctx;
            }
        }
        ~Acceptor()
        {
            if (operator bool()) {
                closesocket(socket);
            }
        }

        operator bool() const { return socket != INVALID_SOCKET; }

        void async_accept() {}
    };
} // namespace NET
} // namespace SL