
#include "Socket_Lite.h"
#include "defs.h"
#include <assert.h>
#if !_WIN32
#include <netinet/ip.h>
#include <sys/epoll.h>
#endif
namespace SL {
namespace NET {

    Listener::Listener(Context &context, PlatformSocket &&acceptsocket, std::function<void(Socket )> &&accepthandler) : 
    ContextImpl_(*context.ContextImpl_), AcceptSocket(std::move(acceptsocket)), AcceptHandler(std::move(accepthandler))
    { 
        Keepgoing = true;
        Runner = std::thread([&]() {
            while (Keepgoing) {
#if _WIN32

                auto handle = ::accept(Acceptor_.AcceptSocket.Handle().value, NULL, NULL);
                if (handle != INVALID_SOCKET) {
                    PlatformSocket s(handle);

                    if (CreateIoCompletionPort((HANDLE)handle, ContextImpl_.getIOHandle(), handle, NULL) == NULL) {
                        continue; // this shouldnt happen but what ever
                    }
                    [[maybe_unused]] auto ret = s.setsockopt(BLOCKINGTag{}, SL::NET::Blocking_Options::NON_BLOCKING);
                    ContextImpl_.RegisterSocket(s.Handle());
                    AcceptHandler(Socket(ContextImpl_, std::move(s)));
                }
#else

                auto handle = ::accept4(AcceptSocket.Handle().value, NULL, NULL, SOCK_NONBLOCK);
                if (handle != INVALID_SOCKET) {
                    PlatformSocket s(handle); 
                    epoll_event ev = {0};
                    ev.events= EPOLLONESHOT | EPOLLRDHUP | EPOLLHUP;  
                    if (epoll_ctl(ContextImpl_.getIOHandle(), EPOLL_CTL_ADD, handle, &ev) == -1) {
                        continue; // this shouldnt happen but what ever
                    }
                    ContextImpl_.RegisterSocket(s.Handle());
                    AcceptHandler(Socket(ContextImpl_, std::move(s)));
                }
#endif
            }
            AcceptHandler = nullptr;
        });
    }
    Listener::~Listener()
    {
        Keepgoing = false;
#if _WIN32
        AcceptSocket.close();
#else
        AcceptSocket.shutdown(ShutDownOptions::SHUTDOWN_READ);
#endif 
        Runner.join(); 
    }
} // namespace NET
} // namespace SL
