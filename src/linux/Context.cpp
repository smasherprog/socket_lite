#include "Context.h"
#include "Listener.h"
#include "Socket.h"
#include <chrono>
#include <string.h>
#include <mutex>

using namespace std::chrono_literals;
namespace SL
{
namespace NET
{
std::shared_ptr<ISocket> Context::CreateSocket()
{
    return std::make_shared<Socket>(this);
}
std::shared_ptr<IListener> Context::CreateListener(std::shared_ptr<ISocket> &&listensocket)
{
    listensocket->setsockopt<SocketOptions::O_BLOCKING>(Blocking_Options::NON_BLOCKING);
    auto addr = listensocket->getsockname();
    if(!addr.has_value()) {
        return std::shared_ptr<IListener>();
    }
    auto listener = std::make_shared<Listener>(this, std::forward<std::shared_ptr<ISocket>>(listensocket), addr.value());
    epoll_event ev = {0};
    ev.data.ptr = &listener->Win_IO_Accept_Context_;
    ev.events = EPOLLIN;
    if(epoll_ctl(iocp.handle, EPOLL_CTL_ADD, listensocket->get_handle(), &ev)  == -1) {
        return std::shared_ptr<Listener>();
    }
    return listener;
}
std::shared_ptr<IContext> CreateContext()
{
    return std::make_shared<Context>();
}
Context::Context()
{
    EventWakeFd = eventfd(0, EFD_NONBLOCK);
    epoll_event ev = {0};
    ev.events = EPOLLIN | EPOLLET;
    ev.data.fd = EventWakeFd;
    if(epoll_ctl(iocp.handle, EPOLL_CTL_ADD, EventWakeFd, &ev)==1) {
        //ERROR
    }
    PendingIO = 0;
}

Context::~Context()
{
    while (PendingIO > 0) {
        std::this_thread::sleep_for(5ms);
    }
    if(EventWakeFd!= -1) {
        eventfd_write(EventWakeFd, 1);//make sure to wake up the threads
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
    if(EventWakeFd!= -1) {
        close(EventWakeFd);
    }
}

void handleaccept(Win_IO_Accept_Context* cont, Context* context, IOCP& iocp)
{
    int listenhandle =-1;
    decltype(cont->completionhandler) handler;
    {
        std::lock_guard<SpinLock> lock(context->AcceptLock);
        if(!cont->completionhandler) {
            return;
        }
        handler = std::move(cont->completionhandler);
        listenhandle = cont->ListenSocket;
        cont->clear();
    }


    sockaddr_in remote= {0};
    socklen_t len = sizeof(remote);
    int i = accept(listenhandle, reinterpret_cast<::sockaddr*>(&remote), &len);
    if(i==-1) {
        if(errno == EAGAIN || errno == EWOULDBLOCK) {
            handler(StatusCode::SC_CLOSED, std::shared_ptr<ISocket>());
        } else {
            handler(TranslateError(), std::shared_ptr<ISocket>());
        }
    } else {
        auto sock(std::make_shared<Socket>(context));
        sock->set_handle(i);
        sock->setsockopt<SocketOptions::O_BLOCKING>(Blocking_Options::NON_BLOCKING);
        epoll_event ev = {0};
        ev.events =  EPOLLOUT | EPOLLIN | EPOLLEXCLUSIVE;
        ev.data.ptr = &sock->WriteContext;
        if(epoll_ctl(iocp.handle, EPOLL_CTL_ADD, i, &ev)  == -1) {
            handler(TranslateError(), std::shared_ptr<ISocket>());
        } else {
            handler(StatusCode::SC_SUCCESS, sock);
        }
    }
}

void Context::run(ThreadCount threadcount)
{


    Threads.reserve(threadcount.value);
    for (auto i = 0; i < threadcount.value; i++) {
        Threads.push_back(std::thread([&] {
            std::vector<epoll_event> epollevents;
            epollevents.resize(MAXEVENTS);
            while (true) {
                auto count = epoll_wait(iocp.handle, epollevents.data(),MAXEVENTS,500);
                if(count==-1) {
                    if(errno == EINTR && PendingIO > 0) {
                        continue;
                    }
                    /*
                    if(EventWakeFd!= -1) {//wake the next thread
                        eventfd_write(EventWakeFd, 1);//make sure to wake up the threads
                    }
                    return;
                    */
                }
                for(auto i=0; i< count ; i++) {
                    if(epollevents[i].data.fd == EventWakeFd) {
                        printf("Wake up received!");
                        if (PendingIO <= 0) {
                            return;
                        }
                        continue;//keep going
                    }
                    auto ctx = static_cast<IO_Context*>(epollevents[i].data.ptr);

                    switch (ctx->IOOperation) {
                    case IO_OPERATION::IoConnect:
                        static_cast<Win_IO_RW_Context *>(ctx)->Socket_->handleconnect();
                        break;
                    case IO_OPERATION::IoAccept:
                        handleaccept(static_cast<Win_IO_Accept_Context *>(ctx), this, iocp);
                        break;
                    case IO_OPERATION::IoRead:
                        static_cast<Win_IO_RW_Context *>(ctx)->Socket_->handlerecv();
                        break;
                    case IO_OPERATION::IoWrite:
                        static_cast<Win_IO_RW_Context *>(ctx)->Socket_->handlewrite();
                        break;
                    default:
                        break;
                    }/*
                    if (--PendingIO <= 0) {
                        if(EventWakeFd!= -1) {//wake the next thread
                            eventfd_write(EventWakeFd, 1);//make sure to wake up the threads
                        }
                        return;
                    }*/
                }
            }
        }));
    }
}
}
}
