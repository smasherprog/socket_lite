#pragma once

#include <WinSock2.h>
#include <Windows.h>
#include <Ws2tcpip.h>
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
    struct SendBuffer {
        size_t totalsent = 0;
        size_t total = 0;
        std::shared_ptr<unsigned char> Buffer;
    };
    enum IO_OPERATION { IoAccept, IoRead, IoWrite };

    class Socket;
    class ISocket;
    class Context;
    struct PER_IO_CONTEXT {
        WSAOVERLAPPED Overlapped = {0};
        WSABUF wsabuf = {0};
        std::mutex SendBuffersLock;
        SendBuffer CurrentBuffer;
        std::vector<SendBuffer> SendBuffers;
        IO_OPERATION IOOperation = IO_OPERATION::IoAccept;
        std::shared_ptr<Socket> Socket;
    };

    PER_IO_CONTEXT *createcontext(NetworkProtocol protocol, Context *context);
    void freecontext(PER_IO_CONTEXT **context);
    bool updateIOCP(SOCKET socket, HANDLE *iocphandle);
    void async_read(PER_IO_CONTEXT *lpOverlapped, std::function<void(const std::shared_ptr<ISocket> &)> &onDisconnection);

} // namespace NET
} // namespace SL