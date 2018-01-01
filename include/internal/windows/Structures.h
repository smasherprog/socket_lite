#pragma once
#define WIN32_LEAN_AND_MEAN
#include <WinSock2.h>
#include <Windows.h>
#include <Ws2tcpip.h>
#include <memory>
#include <mswsock.h>
#include <vector>

namespace SL {
namespace NET {
    struct WSARAII {

        WSADATA wsaData;
        bool good = false;
        WSARAII()
        {
            if (auto ret = WSAStartup(0x202, &wsaData); ret != 0) {
                // error
                good = false;
            }
            good = true;
        }
        ~WSARAII()
        {
            if (auto ret = WSACleanup(); ret != 0) {
                // error
            }
        }
        operator bool() const { return good; }
    };
    struct WSAEvent {

        HANDLE handle = WSA_INVALID_EVENT;
        WSAEvent()
        {
            if (handle = WSACreateEvent(); handle == WSA_INVALID_EVENT) {
                // error
            }
        }
        ~WSAEvent()
        {
            if (operator bool()) {
                WSACloseEvent(handle);
            }
        }
        operator bool() const { return handle != WSA_INVALID_EVENT; }
    };

    struct IOCP {

        HANDLE handle = NULL;
        IOCP()
        {
            if (handle = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0); handle == NULL) {
                // error
            }
        }
        ~IOCP()
        {
            if (operator bool()) {
                CloseHandle(handle);
            }
        }
        operator bool() const { return handle != NULL; }
    };

    struct Socket {
        SOCKET handle = INVALID_SOCKET;
        WSAOVERLAPPED Overlapped = {0};
        Socket()
        {
            if (handle = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_IP, NULL, 0, WSA_FLAG_OVERLAPPED); handle != INVALID_SOCKET) {
                auto nZero = 0;
                if (auto nRet = setsockopt(handle, SOL_SOCKET, SO_SNDBUF, (char *)&nZero, sizeof(nZero)); nRet == SOCKET_ERROR) {
                    close();
                }
                u_long iMode = 1; // set socket for non blocking
                if (auto nRet = ioctlsocket(handle, FIONBIO, &iMode); nRet == NO_ERROR) {
                    close();
                }
            }
        }
        ~Socket() { close(); }
        void close()
        {

            if (operator bool()) {
                closesocket(handle);
            }
            handle = INVALID_SOCKET;
        }
        operator bool() const { return handle != NULL; }
    };

    struct Context {
        WSARAII wsa;
        std::vector<WSAEvent> IO_ServiceRunners;
        IOCP iocp;
        std::vector<std::shared_ptr<Socket>> SocketContexts;

        operator bool() const { return wsa && iocp; }
    };
} // namespace NET
} // namespace SL