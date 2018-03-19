#include "Context.h"
#include "Listener.h"
#include "Socket.h"
#include <chrono>
#include <mutex>
#include <string.h>
#ifndef _WIN32
#include <unistd.h>
#endif

using namespace std::chrono_literals;
namespace SL {
namespace NET {
    std::shared_ptr<ISocket> Context::CreateSocket() { return std::make_shared<Socket>(this); }
    std::shared_ptr<IListener> Context::CreateListener(std::shared_ptr<ISocket> &&listensocket)
    {
        listensocket->setsockopt<SocketOptions::O_BLOCKING>(Blocking_Options::NON_BLOCKING);
        auto addr = listensocket->getsockname();
        if (!addr.has_value()) {
            return std::shared_ptr<IListener>();
        }
        return std::make_shared<Listener>(this, std::forward<std::shared_ptr<ISocket>>(listensocket), addr.value());
    }
    std::shared_ptr<IContext> CreateContext() { return std::make_shared<Context>(); }
    Context::Context()
    {
        IOCPHandle = epoll_create1(0);
        EventWakeFd = eventfd(0, EFD_NONBLOCK);
        if (IOCPHandle == -1 || EventWakeFd == -1) {
            abort();
        }
        epoll_event ev = {0};
        ev.events = EPOLLIN | EPOLLET;
        ev.data.fd = EventWakeFd;
        if (epoll_ctl(IOCPHandle, EPOLL_CTL_ADD, EventWakeFd, &ev) == 1) {
            // ERROR
        }
        PendingIO = 0;
    }

    Context::~Context()
    {
        while (PendingIO > 0) {
            std::this_thread::sleep_for(5ms);
        }
        eventfd_write(EventWakeFd, 1); // make sure to wake up the threads
        for (auto &t : Threads) {

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
        if (EventWakeFd != -1) {
            ::close(EventWakeFd);
        }
        ::close(IOCPHandle);
    }
    void Context::run()
    {
        Threads.reserve(ThreadCount_.value);
        for (auto i = 0; i < ThreadCount_.value; i++) {
            Threads.push_back(std::thread([&] {
                std::vector<epoll_event> epollevents;
                epollevents.resize(MAXEVENTS);
                while (true) {
                    auto count = epoll_wait(IOCPHandle, epollevents.data(), MAXEVENTS, 500);
                    if (count == -1) {
                        if (errno == EINTR && PendingIO > 0) {
                            continue;
                        }
                    }
                    for (auto i = 0; i < count; i++) {
                        if (PendingIO <= 0) {
                            eventfd_write(EventWakeFd, 1); // make sure to wake up the threads
                            return;
                        }
                        auto ctx = static_cast<IO_Context *>(epollevents[i].data.ptr);

                        switch (ctx->IOOperation) {
                        case IO_OPERATION::IoInitConnect:
                            Socket::init_connect(true, static_cast<Win_IO_Connect_Context *>(ctx));
                            break;
                        case IO_OPERATION::IoConnect:
                            Socket::continue_connect(true, static_cast<Win_IO_Connect_Context *>(ctx));
                            break;
                        case IO_OPERATION::IoStartAccept:
                            Listener::start_accept(true, static_cast<Win_IO_Accept_Context *>(ctx));
                            break;
                        case IO_OPERATION::IoAccept:
                            Listener::handle_accept(true, static_cast<Win_IO_Accept_Context *>(ctx));
                            break;
                        case IO_OPERATION::IoRead:
                        case IO_OPERATION::IoWrite:
                            Socket::continue_io(true, static_cast<Win_IO_RW_Context *>(ctx));
                            break;
                        default:
                            break;
                        }
                        if (--PendingIO <= 0) {
                            eventfd_write(EventWakeFd, 1); // make sure to wake up the threads
                            return;
                        }
                    }
                }
            }));
        }
    }
} // namespace NET
} // namespace SL
