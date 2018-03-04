#include "Context.h"
#include "Listener.h"
#include "Socket.h"
#include <chrono>
#include <string.h>

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
    PendingIO = 0;
}

Context::~Context()
{
    KeepRunning = false;
    while (PendingIO != 0) {
        std::this_thread::sleep_for(1ms);
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
}
void Context::handleaccept(Win_IO_Accept_Context* context)
{
    auto handle(std::move(context->completionhandler));
    context->clear();
    sockaddr_in remote= {0};
    socklen_t len = sizeof(remote);
    int i = accept(context->ListenSocket, reinterpret_cast<::sockaddr*>(&remote), &len);
    if(i==-1) {
        if(errno == EAGAIN || errno == EWOULDBLOCK) {
            handle(StatusCode::SC_CLOSED, std::shared_ptr<ISocket>());
        } else {
            handle(TranslateError(), std::shared_ptr<ISocket>());
        }
    } else {
        auto sock(std::make_shared<Socket>(this));
        sock->set_handle(i);
        sock->setsockopt<SocketOptions::O_BLOCKING>(Blocking_Options::NON_BLOCKING);
        epoll_event ev = {0};
        if(epoll_ctl(iocp.handle, EPOLL_CTL_ADD, i, &ev)  == -1) {
            handle(TranslateError(), std::shared_ptr<ISocket>());
        }
        handle(StatusCode::SC_SUCCESS, sock);
    }
}
void Context::handleconnect(Win_IO_RW_Context* context)
{
    context->Socket_->handleconnect();
}
void Context::handlerecv(Win_IO_RW_Context* context)
{
    context->Socket_->onRecvReady();
}
void Context::handlewrite(Win_IO_RW_Context* context)
{
    context->Socket_->onSendReady();
}
void Context::run(ThreadCount threadcount)
{
    Threads.reserve(threadcount.value);
    for (auto i = 0; i < threadcount.value; i++) {
        Threads.push_back(std::thread([&] {
            std::vector<epoll_event> epollevents;
            epollevents.resize(MAXEVENTS);
            while (true) {
                memset(epollevents.data(), 0, sizeof(epoll_event)*MAXEVENTS);
                auto count = epoll_wait(iocp.handle, epollevents.data(),MAXEVENTS, -1 );
                for(auto i=0; i< count ; i++) {
                    auto ctx = static_cast<IO_Context*>(epollevents[i].data.ptr);
                    switch (ctx->IOOperation) {
                    case IO_OPERATION::IoConnect:
                        handleconnect(static_cast<Win_IO_RW_Context *>(ctx));
                        break;
                    case IO_OPERATION::IoAccept:
                        handleaccept(static_cast<Win_IO_Accept_Context *>(ctx));
                        break;
                    case IO_OPERATION::IoRead:
                        handlerecv(static_cast<Win_IO_RW_Context *>(ctx));
                        break;
                    case IO_OPERATION::IoWrite:
                        handlewrite(static_cast<Win_IO_RW_Context *>(ctx));
                        break;
                    default:
                        break;
                    }
                    if (--PendingIO == 0) {
                        return;
                    }
                }
            }
        }));
    }
}
}
}
