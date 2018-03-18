#pragma once

#include <WinSock2.h>
#include <Windows.h>
#include <Ws2tcpip.h>
#include <atomic>
#include <functional>
#include <iostream>
#include <mswsock.h>
#include <mutex>

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
        IOCP(DWORD numberOfConcurrentThreads)
        {
            if (handle = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, numberOfConcurrentThreads); handle == NULL) {
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
    enum IO_OPERATION { IoNone, IoInitConnect, IoConnect, IoStartAccept, IoAccept, IoRead, IoWrite };
    class Socket;
    class Context;
    struct Win_IO_Context {
        WSAOVERLAPPED Overlapped = {0};
        IO_OPERATION IOOperation = IO_OPERATION::IoNone;
    };

    struct Win_IO_Connect_Context : Win_IO_Context {
        SL::NET::sockaddr address;
        std::function<void(StatusCode)> completionhandler;
        Socket *Socket_ = nullptr;
        Context *Context_ = nullptr;
    };

    struct Win_IO_Accept_Context : Win_IO_Context {
        char Buffer[(sizeof(SOCKADDR_STORAGE) + 16) * 2];
        AddressFamily Family = AddressFamily::IPV4;
        Context *Context_ = nullptr;
        std::shared_ptr<Socket> Socket_;
        SOCKET ListenSocket = INVALID_SOCKET;
        std::function<void(StatusCode, const std::shared_ptr<ISocket> &)> completionhandler;
    };
    struct RW_CompletionHandler {
        RW_CompletionHandler() { Completed = 1; }
        std::function<void(StatusCode, size_t)> completionhandler;
        std::atomic<int> Completed;
        void handle(StatusCode code, size_t bytes, bool lockneeded)
        {
            if (lockneeded) {
                if (Completed.fetch_sub(1, std::memory_order_relaxed) == 1) {
                    completionhandler(code, bytes);
                }
            }
            else {
                completionhandler(code, bytes);
            }
        }
    };
    struct Win_IO_RW_Context : Win_IO_Context {
        size_t transfered_bytes = 0;
        size_t bufferlen = 0;
        Context *Context_ = nullptr;
        Socket *Socket_ = nullptr;
        unsigned char *buffer = nullptr;
        std::shared_ptr<RW_CompletionHandler> completionhandler;
    };
} // namespace NET
} // namespace SL
