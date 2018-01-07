#pragma once

#include <WinSock2.h>
#include <Windows.h>
#include <Ws2tcpip.h>
#include <functional>
#include <memory>
#include <mswsock.h>
#include <mutex>
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
    enum IO_OPERATION { IoConnect, IoAccept, IoRead, IoWrite };

    class Socket;
    struct PER_IO_CONTEXT {
        WSAOVERLAPPED Overlapped = {0};
        IO_OPERATION IOOperation = IO_OPERATION::IoAccept;
        WSABUF wsabuf = {0};
        size_t transfered_bytes = 0;
        size_t bufferlen = 0;
        unsigned char *buffer = nullptr;
        std::function<void(Bytes_Transfered)> completionhandler;
        std::shared_ptr<ISocket> Socket_;
    };

    inline bool updateIOCP(SOCKET socket, HANDLE *iocphandle)
    {
        if (iocphandle && *iocphandle != NULL) {
            if (auto ctx = CreateIoCompletionPort((HANDLE)socket, *iocphandle, NULL, 0); ctx == NULL) {
                return false;
            }
            else {
                *iocphandle = ctx;
                return true;
            }
        }
        return false;
    }

} // namespace NET
} // namespace SL