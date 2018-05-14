
#include "Socket_Lite.h"
#include "defs.h"
#include <assert.h>
#if !_WIN32
#include <netinet/ip.h>
#include <sys/epoll.h>
#endif
namespace SL {
namespace NET {

    Listener::Listener(Context &context, Acceptor &&acceptor) : ContextImpl_(*context.ContextImpl_), Acceptor_(std::move(acceptor))
    { 
        Keepgoing = true;
        Runner = std::thread([&]() {
            while (Keepgoing) 
            {
#if _WIN32
            
                auto handle = ::accept(Acceptor_.AcceptSocket.Handle().value, NULL, NULL);
                if (handle != INVALID_SOCKET) {
                    PlatformSocket s(handle);

                    auto& iodata = ContextImpl_.getIOData();
                    if (CreateIoCompletionPort((HANDLE)handle, iodata.getIOHandle(), NULL, NULL) == NULL) {
                        continue; // this shouldnt happen but what ever
                    }
                    [[maybe_unused]] auto ret = s.setsockopt(BLOCKINGTag{}, SL::NET::Blocking_Options::NON_BLOCKING);
                    
                    Acceptor_.AcceptHandler(Socket(iodata, std::move(s)));
                }
#else
                
                auto handle = ::accept4(Acceptor_.AcceptSocket.Handle().value, NULL, NULL,SOCK_NONBLOCK);
                if (handle != INVALID_SOCKET) {
                    
                    auto& iodata = ContextImpl_.getIOData();
                    epoll_event ev = {0};
                    ev.events = EPOLLONESHOT;
                    if (epoll_ctl(iodata.getIOHandle(), EPOLL_CTL_ADD, handle, &ev) == -1) {
                        continue; // this shouldnt happen but what ever
                    }
                    
                    Acceptor_.AcceptHandler(Socket(iodata, PlatformSocket(handle)));
                } 
#endif
            }
            Acceptor_.AcceptHandler = nullptr;
        });
    }
    Listener::~Listener() {       
        Keepgoing = false;
        Acceptor_.AcceptSocket.close();
        Runner.join(); 
    }

 
 
} // namespace NET
} // namespace SL
