#pragma once

#include "common/Structures.h"
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <errno.h>


namespace SL
{
namespace NET
{
struct IOCP {

    int handle = -1;
    IOCP() {
        if (handle = epoll_create1(0); handle == -1) {
            std::cerr << "epoll_create1() failed : " << errno() << std::endl;
        }
    }
    ~IOCP() {
        if (operator bool()) {
            close(handle);
        }
    }
    operator bool() const {
        return handle != -1;
    }
};
class Socket;
struct Win_IO_Context {

    IO_OPERATION IOOperation = IO_OPERATION::IoNone;
};
struct Win_IO_Accept_Context : Win_IO_Context {
    std::shared_ptr<Socket> Socket_;
    int ListenSocket = -1;
    std::function<void(bool)> completionhandler;
    void clear() {
        IOOperation = IO_OPERATION::IoNone;
        Socket_.reset();
        completionhandler = nullptr;
        if(ListenSocket>=0) {
            closesocket(ListenSocket);
        }
    }
};
struct Win_IO_RW_Context : Win_IO_Context {
    Bytes_Transfered transfered_bytes = 0;
    Bytes_Transfered bufferlen = 0;
    unsigned char *buffer = nullptr;
    std::function<void(Bytes_Transfered)> completionhandler;
    void clear() {
        IOOperation = IO_OPERATION::IoNone;
        transfered_bytes = 0;
        bufferlen = 0;
        buffer = nullptr;
        completionhandler = nullptr;
    }
};
} // namespace NET
} // namespace SL
