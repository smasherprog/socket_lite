#include <chrono>
#include <defs.h>
#include <mutex>
#include <string.h>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <unistd.h>

using namespace std::chrono_literals;
namespace SL
{
namespace NET
{ 
Context::Context(ThreadCount threadcount)
    : ThreadCount_(threadcount)
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
        abort();
    }
    PendingIO = 0;
}

Context::~Context()
{
    while (PendingIO > 0) {
        std::this_thread::sleep_for(5ms);
        eventfd_write(EventWakeFd, 1); // make sure to wake up the threads
    }

    for (auto &t : Threads) {

        if (t.joinable()) {
            // destroying myself
            if (t.get_id() == std::this_thread::get_id()) {
                t.detach();
            } else {
                t.join();
            }
        }
    }
    if (EventWakeFd != -1) {
        ::close(EventWakeFd);
    }
    ::close(IOCPHandle);
}
const auto MAXEVENTS = 10;
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
                    if (epollevents[i].data.fd != EventWakeFd) {
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
                        if (--PendingIO <= 0) {
                            return;
                        }
                    }
                }
            }
        }));
    }
}
} // namespace NET
} // namespace SL
