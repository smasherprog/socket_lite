#pragma once
#include "Socket_Lite.h"
#include "common/Structures.h"
#include <WinSock2.h>
#include <Windows.h>
#include <Ws2tcpip.h>
#include <functional>
#include <iostream>
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

    struct IOCP {

        HANDLE handle = NULL;
        IOCP()
        {
            if (handle = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0); handle == NULL) {
                std::cerr << "CreateIoCompletionPort() failed to create I/O completion port: " << GetLastError() << std::endl;
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
    class Socket;
    struct WinIOContext {
        WSAOVERLAPPED Overlapped = {0};
        IO_OPERATION IOOperation = IO_OPERATION::IoNone;
    };
    struct Win_IO_Accept_Context : WinIOContext {
        std::shared_ptr<Socket> Socket_;
        SOCKET ListenSocket = INVALID_SOCKET;
        std::function<void(bool)> completionhandler;
    };
    struct Win_IO_Connect_Context : WinIOContext {
        HANDLE iocp = NULL;
        LPFN_CONNECTEX ConnectEx_;
        std::vector<sockaddr> RemainingAddresses;
        std::function<ConnectSelection(ConnectionAttemptStatus, sockaddr &)> completionhandler;
    };
    struct Win_IO_RW_Context : WinIOContext {
        WSABUF wsabuf = {0};
        Bytes_Transfered transfered_bytes = 0;
        Bytes_Transfered bufferlen = 0;
        unsigned char *buffer = nullptr;
        std::function<void(Bytes_Transfered)> completionhandler;
        void clear()
        {
            Overlapped = {0};
            IOOperation = IO_OPERATION::IoNone;
            wsabuf = {0};
            transfered_bytes = 0;
            bufferlen = 0;
            delete buffer;
            buffer = nullptr;
            completionhandler = nullptr;
        }
    };
} // namespace NET
} // namespace SL