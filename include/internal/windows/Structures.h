#pragma once
#define WIN32_LEAN_AND_MEAN
#include <WinSock2.h>
#include <Windows.h>
#include <Ws2tcpip.h>
#include <mswsock.h>
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

    struct SocketContext {
        SOCKET Socket;
    };

} // namespace NET
} // namespace SL