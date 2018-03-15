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

    struct Win_IO_Context {
        WSAOVERLAPPED Overlapped = {0};
        IO_OPERATION IOOperation = IO_OPERATION::IoNone;
    };

    struct Win_IO_Connect_Context : Win_IO_Context {
        SL::NET::sockaddr address;
        std::function<void(StatusCode)> completionhandler;
        Socket *Socket_ = nullptr;
        void clear()
        {
            completionhandler = nullptr;
            Socket_ = nullptr;
        }
    };
    struct Win_IO_Accept_Context : Win_IO_Context {
        std::shared_ptr<Socket> Socket_;
        SOCKET ListenSocket = INVALID_SOCKET;
        std::function<void(StatusCode, const std::shared_ptr<ISocket> &)> completionhandler;
    };
    struct RW_CompletionHandler {
        RW_CompletionHandler()
        {
            Completed = 1;
            RefCount = 0;
        }
        std::function<void(StatusCode, size_t)> completionhandler;
        std::atomic<int> Completed;
        std::atomic<int> RefCount;
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
        void clear()
        {
            RefCount = 0;
            Completed = 1;
            completionhandler = nullptr;
        }
    };
    struct Win_IO_RW_Context : Win_IO_Context {
        size_t transfered_bytes = 0;
        size_t bufferlen = 0;
        unsigned char *buffer = nullptr;
        RW_CompletionHandler *completionhandler = nullptr;
        void clear()
        {
            transfered_bytes = 0;
            bufferlen = 0;
            buffer = nullptr;
            completionhandler = nullptr;
            Overlapped = {0};
            IOOperation = IO_OPERATION::IoNone;
        }
    };
    struct Win_IO_RW_ContextCRUD {
        Win_IO_RW_Context *newObj() { return new Win_IO_RW_Context(); }
        void deleteObj(Win_IO_RW_Context *p) { delete p; }
        void clearObj(Win_IO_RW_Context *p) { p->clear(); }
    };
    struct RW_CompletionHandlerCRUD {
        RW_CompletionHandler *newObj() { return new RW_CompletionHandler(); }
        void deleteObj(RW_CompletionHandler *p) { delete p; }
        void clearObj(RW_CompletionHandler *p) { p->clear(); }
    };
} // namespace NET
} // namespace SL
