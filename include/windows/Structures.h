#pragma once

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
                //  std::cerr << "CreateIoCompletionPort() failed to create I/O completion port: " << GetLastError() << std::endl;
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

#ifdef WIN32
    inline StatusCode TranslateError(int *errcode = nullptr)
    {
        auto originalerr = WSAGetLastError();
        auto errorcode = errcode != nullptr ? *errcode : originalerr;
        switch (errorcode) {
        case WSAECONNRESET:
            return StatusCode::SC_ECONNRESET;
        case WSAETIMEDOUT:
        case WSAECONNABORTED:
            return StatusCode::SC_ETIMEDOUT;
        case WSAEWOULDBLOCK:
            return StatusCode::SC_EWOULDBLOCK;

        default:
            return StatusCode::SC_CLOSED;
        };
    }
#else
    inline SocketErrors TranslateError(int *errcode = nullptr)
    {
        switch (errorcode) {

        default:
            return SocketErrors::SE_ECONNRESET;
        };
    }
#endif

    class Socket;
    struct Win_IO_Context {
        WSAOVERLAPPED Overlapped = {0};
        IO_OPERATION IOOperation = IO_OPERATION::IoNone;
    };
    struct Win_IO_Accept_Context : Win_IO_Context {
        std::shared_ptr<Socket> Socket_;
        SOCKET ListenSocket = INVALID_SOCKET;
        std::function<void(StatusCode, const std::shared_ptr<ISocket> &)> completionhandler;
        void clear()
        {
            Overlapped = {0};
            IOOperation = IO_OPERATION::IoNone;
            Socket_.reset();
            completionhandler = nullptr;
        }
    };
    struct Win_IO_RW_Context : Win_IO_Context {
        size_t transfered_bytes = 0;
        size_t bufferlen = 0;
        unsigned char *buffer = nullptr;
        std::function<void(StatusCode, size_t)> completionhandler;
        void clear()
        {
            Overlapped = {0};
            IOOperation = IO_OPERATION::IoNone;
            transfered_bytes = 0;
            bufferlen = 0;
            buffer = nullptr;
            completionhandler = nullptr;
        }
    };
} // namespace NET
} // namespace SL