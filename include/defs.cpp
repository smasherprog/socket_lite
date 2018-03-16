#pragma once
#include "defs.h"

#if defined(WINDOWS) || defined(_WIN32)
#else

#include <errno.h>
#include <sys/epoll.h>

#endif
namespace SL {
namespace NET {
#ifdef _WIN32
    IOCP::IOCP(int numberOfConcurrentThreads)
    {
        if (handle = ::CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, numberOfConcurrentThreads); handle == NULL) {
            //  std::cerr << "CreateIoCompletionPort() failed to create I/O completion port: " << GetLastError() << std::endl;
        }
    }
    IOCP::~IOCP()
    {
        if (operator bool()) {
            ::CloseHandle(handle);
        }
    }
    IOCP::operator bool() const { return handle != NULL; }
    StatusCode TranslateError(int *errcode)
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
    IOCP::IOCP(int numberOfConcurrentThreads) { handle = epoll_create1(0); }
    IOCP::~IOCP()
    {
        if (operator bool()) {
            ::close(handle);
            handle = -1;
        }
    }
    IOCP::operator bool() const { return handle != -1; }
    StatusCode TranslateError(int *errcode)
    {
        auto originalerror = errno;
        auto errorcode = errcode != nullptr ? *errcode : originalerror;
        switch (errorcode) {

        default:
            return StatusCode::SC_CLOSED;
        };
    }
#endif

} // namespace NET
} // namespace SL
