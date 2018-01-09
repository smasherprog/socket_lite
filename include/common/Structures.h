#pragma once
#include "Socket_Lite.h"
#include <WinSock2.h>
#include <Windows.h>
#include <Ws2tcpip.h>
#include <functional>
#include <iostream>
#include <mswsock.h>

namespace SL {
namespace NET {

    enum IO_OPERATION { IoConnect, IoAccept, IoRead, IoWrite };

    struct Socket_IO_Context {
        Bytes_Transfered transfered_bytes = 0;
        Bytes_Transfered bufferlen = 0;
        unsigned char *buffer = nullptr;
        std::function<void(Bytes_Transfered)> completionhandler;
    };
    template <typename T> struct Node {
        T *next = nullptr;
        T *prev = nullptr;
    };

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

    struct Win_IO_Context {
        WSAOVERLAPPED Overlapped = {0};
        IO_OPERATION IOOperation = IO_OPERATION::IoAccept;
        WSABUF wsabuf = {0};
        Socket_IO_Context IO_Context;
        void clear()
        {
            Overlapped = {0};
            IOOperation = IO_OPERATION::IoAccept;
            wsabuf = {0};
            IO_Context.transfered_bytes = 0;
            IO_Context.bufferlen = 0;
            IO_Context.buffer = nullptr;
            IO_Context.completionhandler = nullptr;
        }
    };
    struct Win_IO_Context_List : Win_IO_Context, Node<Win_IO_Context_List> {
    };
} // namespace NET
} // namespace SL