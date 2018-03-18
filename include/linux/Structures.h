#pragma once
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <errno.h>


namespace SL
{
namespace NET
{
const int MAXEVENTS =8;
struct IOCP {

    int handle = -1;
    IOCP() {
        handle = epoll_create1(0);
    }
    ~IOCP() {
        close();
    }
    void close() {
        if (operator bool()) {
            ::close(handle);
            handle = -1;
        }
    }
    operator bool() const {
        return handle != -1;
    }
};

inline StatusCode TranslateError(int *errcode = nullptr)
{
    auto originalerror  = errno;
    auto errorcode = errcode != nullptr ? *errcode : originalerror;
    switch (errorcode) {

    default:
        return StatusCode::SC_CLOSED;
    };
}

enum IO_OPERATION { IoNone, IoConnect, IoAccept, IoRead, IoWrite };
class Socket;
struct IO_Context {
    IO_OPERATION IOOperation = IO_OPERATION::IoNone;
    Socket* Socket_=nullptr;
};
struct Win_IO_Accept_Context:IO_Context  {
    int ListenSocket = -1;
    std::function<void(StatusCode, const std::shared_ptr<ISocket> &)> completionhandler;
    void clear() {
        IOOperation = IO_OPERATION::IoNone;
        completionhandler = nullptr;
    }
};
struct Win_IO_RW_Context :IO_Context  {
    size_t transfered_bytes = 0;
    size_t bufferlen = 0;
    unsigned char *buffer = nullptr;
    std::function<void(StatusCode, size_t)> completionhandler;
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
