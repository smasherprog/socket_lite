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

#include <iostream>

using namespace std::chrono_literals;
namespace SL {
namespace NET {

    Context::Context(ThreadCount threadcount)
    {
        ContextImpl_ = new ContextImpl(threadcount);
#if _WIN32
        if (WSAStartup(0x202, &ContextImpl_->wsaData) != 0) {
            abort();
        }
        ContextImpl_->IOCPHandle = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, threadcount.value);

        auto handle = WSASocketW(AF_INET, SOCK_STREAM, IPPROTO_IP, NULL, 0, WSA_FLAG_OVERLAPPED);
        GUID guid = WSAID_CONNECTEX;
        DWORD bytes = 0;
        WSAIoctl(handle, SIO_GET_EXTENSION_FUNCTION_POINTER, &guid, sizeof(guid), &ContextImpl_->ConnectEx_, sizeof(ContextImpl_->ConnectEx_), &bytes,
                 NULL, NULL);
        closesocket(handle);
        if (ContextImpl_->IOCPHandle == NULL || ContextImpl_->ConnectEx_ == nullptr) {
            abort();
        }
#else
        ContextImpl_->IOCPHandle = epoll_create1(0);
        ContextImpl_->EventWakeFd = eventfd(0, EFD_NONBLOCK);
        if (ContextImpl_->IOCPHandle == -1 || ContextImpl_->EventWakeFd == -1) {
            abort();
        }
        epoll_event ev = {0};
        ev.events = EPOLLIN | EPOLLET;
        ev.data.fd = ContextImpl_->EventWakeFd;
        if (epoll_ctl(ContextImpl_->IOCPHandle, EPOLL_CTL_ADD, ContextImpl_->EventWakeFd, &ev) == 1) {
            abort();
        }
#endif
    }

    Context::~Context()
    {
        while (ContextImpl_->getPendingIO() > 0) {
            std::this_thread::sleep_for(10ms);
#ifndef _WIN32
        eventfd_write(ContextImpl_->EventWakeFd, 1);
#endif
        }
        wakeup();
        for (auto &t : ContextImpl_->Threads) {

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
        CloseHandle(ContextImpl_->IOCPHandle);
        WSACleanup();
#else

        if (ContextImpl_->EventWakeFd != -1) {
            ::close(ContextImpl_->EventWakeFd);
        }
        ::close(ContextImpl_->IOCPHandle);
#endif
        delete ContextImpl_;
    }
    void Context::wakeup()
    {
#if _WIN32
        PostQueuedCompletionStatus(ContextImpl_->IOCPHandle, 0, (DWORD)NULL, NULL);
#else
        eventfd_write(ContextImpl_->EventWakeFd, 1);
#endif
    }
    const auto MAXEVENTS = 10;
    void Context::run()
    {
        ContextImpl_->Threads.reserve(ContextImpl_->getThreadCount());
        for (auto i = 0; i < ContextImpl_->getThreadCount(); i++) {
            ContextImpl_->Threads.push_back(std::thread([&] {
               
#if _WIN32
                while (true) {
                    DWORD numberofbytestransfered = 0;
                    Win_IO_Context *overlapped = nullptr;
                    void *completionkey = nullptr;

                    auto bSuccess = GetQueuedCompletionStatus(ContextImpl_->IOCPHandle, &numberofbytestransfered, (PDWORD_PTR)&completionkey,
                                                              (LPOVERLAPPED *)&overlapped, INFINITE) == TRUE;

                    if (ContextImpl_->getPendingIO() <= 0) {
                        wakeup();
                        return;
                    }
                    // std::cout << " type " << overlapped->IOOperation << std::endl;

                    switch (overlapped->IOOperation) {

                    case IO_OPERATION::IoConnect:
                        continue_connect(bSuccess, overlapped);
                        break;
                    case IO_OPERATION::IoRead:
                    case IO_OPERATION::IoWrite:
                        overlapped->transfered_bytes += numberofbytestransfered;
                        if (numberofbytestransfered == 0 && overlapped->bufferlen != 0 && bSuccess) {
                            bSuccess = WSAGetLastError() == WSA_IO_PENDING;
                        }
                        continue_io(bSuccess, overlapped);
                        break;
                    default:
                        break;
                    }
                    if (ContextImpl_->DecrementPendingIO() <= 0) {
                        wakeup();
                        return;
                    }
                }
#else
                std::vector<epoll_event> epollevents;
                epollevents.resize(MAXEVENTS);
                while (true) {
                    auto count = epoll_wait(ContextImpl_->IOCPHandle, epollevents.data(), MAXEVENTS, -1);

                    for (auto i = 0; i < count; i++) {
                        if (epollevents[i].data.fd != ContextImpl_->EventWakeFd) {
                            auto ctx = static_cast<Win_IO_Context *>(epollevents[i].data.ptr);

                            switch (ctx->IOOperation) {
                            case IO_OPERATION::IoConnect:
                                continue_connect(true, ctx);
                                break;
                            case IO_OPERATION::IoRead:
                            case IO_OPERATION::IoWrite:
                                continue_io(true, ctx);
                                break;
                            default:
                                break;
                            }
                            if (ContextImpl_->DecrementPendingIO() <= 0) {
                                wakeup();
                                return;
                            }
                        }
                    }

                    if (ContextImpl_->getPendingIO() <= 0) {
                        wakeup();
                        return;
                    }
                }
#endif     
        }));
        }
    }
} // namespace NET
} // namespace SL
