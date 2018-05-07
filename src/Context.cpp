#include "Socket_Lite.h"
#include "defs.h"

#include <algorithm>
#include <chrono>
#if _WIN32
#include <Ws2tcpip.h>
#else
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <unistd.h>
#endif
using namespace std::chrono_literals;
namespace SL {
namespace NET {
    Context::Context(ThreadCount threadcount) : ContextImpl_(threadcount)
    {

#if _WIN32
        if (WSAStartup(0x202, &ContextImpl_.wsaData) != 0) {
            abort();
        }
        ContextImpl_.IOCPHandle = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, threadcount.value);

        auto handle = WSASocketW(AF_INET, SOCK_STREAM, IPPROTO_IP, NULL, 0, WSA_FLAG_OVERLAPPED);
        GUID guid = WSAID_CONNECTEX;
        DWORD bytes = 0;
        WSAIoctl(handle, SIO_GET_EXTENSION_FUNCTION_POINTER, &guid, sizeof(guid), &ContextImpl_.ConnectEx_, sizeof(ContextImpl_.ConnectEx_), &bytes,
                 NULL, NULL);
        closesocket(handle);
        if (ContextImpl_.IOCPHandle == NULL || ContextImpl_.ConnectEx_ == nullptr) {
            abort();
        }
#else
        ContextImpl_.IOCPHandle = epoll_create1(0);
        ContextImpl_.EventWakeFd = eventfd(0, EFD_NONBLOCK);
        if (ContextImpl_.IOCPHandle == -1 || ContextImpl_.EventWakeFd == -1) {
            abort();
        }
        epoll_event ev = {0};
        ev.events = EPOLLIN | EPOLLET;
        ev.data.fd = EventWakeFd;
        if (epoll_ctl(ContextImpl_.IOCPHandle, EPOLL_CTL_ADD, EventWakeFd, &ev) == 1) {
            abort();
        }
#endif
    }

    Context::~Context()
    {
        while (ContextImpl_.getPendingIO() > 0) {
            std::this_thread::sleep_for(5ms);
            // make sure to wake up the threads
#ifndef _WIN32
            eventfd_write(EventWakeFd, 1);
#endif
        }
#if _WIN32
        PostQueuedCompletionStatus(ContextImpl_.IOCPHandle, 0, (DWORD)NULL, NULL);
#endif
        for (auto &t : ContextImpl_.Threads) {

            if (t.joinable()) {
                // destroying myself
                if (t.get_id() == std::this_thread::get_id()) {
                    t.detach();
                }
                else {
                    t.join();
                }
            }
        }

#if _WIN32
        CloseHandle(ContextImpl_.IOCPHandle);
        WSACleanup();
#else

        if (ContextImpl_.EventWakeFd != -1) {
            ::close(ContextImpl_.EventWakeFd);
        }
        ::close(ContextImpl_.IOCPHandle);
#endif
    }
    const auto MAXEVENTS = 10;
    void Context::run()
    {
        ContextImpl_.Threads.reserve(ContextImpl_.getThreadCount());
        for (auto i = 0; i < ContextImpl_.getThreadCount(); i++) {
            ContextImpl_.Threads.push_back(std::thread([&] {
#if _WIN32
                while (true) {
                    DWORD numberofbytestransfered = 0;
                    INTERNAL::Win_IO_Context *overlapped = nullptr;
                    void *completionkey = nullptr;

                    auto bSuccess = GetQueuedCompletionStatus(ContextImpl_.IOCPHandle, &numberofbytestransfered, (PDWORD_PTR)&completionkey,
                                                              (LPOVERLAPPED *)&overlapped, INFINITE) == TRUE;

                    if (ContextImpl_.getPendingIO() <= 0) {
                        PostQueuedCompletionStatus(ContextImpl_.IOCPHandle, 0, (DWORD)NULL, NULL);
                        return;
                    }
                    switch (overlapped->IOOperation) {

                    case INTERNAL::IO_OPERATION::IoConnect:
                        continue_connect(bSuccess, static_cast<Win_IO_Connect_Context *>(overlapped));
                        break;
                    case INTERNAL::IO_OPERATION::IoRead:
                    case INTERNAL::IO_OPERATION::IoWrite:
                        static_cast<INTERNAL::Win_IO_RW_Context *>(overlapped)->transfered_bytes += numberofbytestransfered;
                        if (numberofbytestransfered == 0 && static_cast<INTERNAL::Win_IO_RW_Context *>(overlapped)->bufferlen != 0 && bSuccess) {
                            bSuccess = WSAGetLastError() == WSA_IO_PENDING;
                        }
                        continue_io(bSuccess, *static_cast<INTERNAL::Win_IO_RW_Context *>(overlapped));
                        break;
                    default:
                        break;
                    }
                    if (ContextImpl_.DecrementPendingIO() <= 0) {
                        PostQueuedCompletionStatus(ContextImpl_.IOCPHandle, 0, (DWORD)NULL, NULL);
                        return;
                    }
                }
#else
                std::vector<epoll_event> epollevents;
                epollevents.resize(MAXEVENTS);
                while (true) {
                    auto count = epoll_wait(ContextImpl_.IOCPHandle, epollevents.data(), MAXEVENTS, 500);
                    if (count == -1) {
                        if (errno == EINTR && ContextImpl_.PendingIO > 0) {
                            continue;
                        }
                    }
                    for (auto i = 0; i < count; i++) {
                        if (epollevents[i].data.fd != ContextImpl_.EventWakeFd) {
                            auto ctx = static_cast<INTERNAL::Win_IO_Context *>(epollevents[i].data.ptr);
                            switch (ctx->IOOperation) {
                            case INTERNAL::IO_OPERATION::IoConnect:
                                continue_connect(true, static_cast<INTERNAL::Win_IO_RW_Context *>(ctx));
                                break;
                            case INTERNAL::IO_OPERATION::IoRead:
                            case INTERNAL::IO_OPERATION::IoWrite:
                                continue_io(true, static_cast<INTERNAL::Win_IO_RW_Context *>(ctx), PendingIO, IOCPHandle);
                                break;
                            default:
                                break;
                            }
                            if (--ContextImpl_.PendingIO <= 0) {
                                return;
                            }
                        }
                    }
                }
#endif
            }));
        }
    }
} // namespace NET
} // namespace SL
