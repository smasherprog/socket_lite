
#include "Internal/Context.h"
#include "Socket_Lite.h"
#include <assert.h>

#if !_WIN32
#include <netinet/ip.h>
#include <sys/epoll.h>
#endif
#include <iostream>

namespace SL {
namespace NET {

    class Listener {
      public:
        Context &ContextImpl_;
        PlatformSocket ListenSocket;
        std::function<void(Socket)> AcceptHandler;
        AddressFamily AddressFamily_;
        std::thread Runner;
        bool Keepgoing;
        Listener(Context &c, PlatformSocket &&acceptsocket, std::function<void(Socket)> &&accepthandler)
            : ContextImpl_(c), ListenSocket(std::move(acceptsocket)), AcceptHandler(std::move(accepthandler))
        {
            Keepgoing = true;
            Runner = std::thread([&]() {
                while (Keepgoing) {
#if _WIN32

                    auto handle = ::accept(ListenSocket.Handle().value, NULL, NULL);
                    if (handle != INVALID_SOCKET) {
                        PlatformSocket s(handle);

                        if (CreateIoCompletionPort((HANDLE)handle, ContextImpl_.getIOHandle(), handle, NULL) == NULL) {
                            continue; // this shouldnt happen but what ever
                        }
                        [[maybe_unused]] auto ret = s.setsockopt(BLOCKINGTag{}, SL::NET::Blocking_Options::NON_BLOCKING);
                        AcceptHandler(Socket(ContextImpl_, std::move(s)));
                    }
#else

                    auto handle = ::accept4(AcceptSocket.Handle().value, NULL, NULL, SOCK_NONBLOCK);
                    if (handle != INVALID_SOCKET) {
                        PlatformSocket s(handle);
                        epoll_event ev = {0};
                        ev.events = EPOLLONESHOT;
                        if (epoll_ctl(ContextImpl_.getIOHandle(), EPOLL_CTL_ADD, handle, &ev) == -1) {
                            continue; // this shouldnt happen but what ever
                        }
                        AcceptHandler(Socket(ContextImpl_, std::move(s)));
                    }
#endif
                }
                AcceptHandler = nullptr;
            });
        }

        ~Listener()
        {
            Keepgoing = false;
            if (ListenSocket) {
#if _WIN32
                ListenSocket.close();
#else
                ListenSocket.shutdown(ShutDownOptions::SHUTDOWN_READ);
#endif
            } 
            if (Runner.joinable()) { 
                Runner.join();
            }
        }
    };

    std::shared_ptr<INetworkConfig> NetworkConfig::AddListener(PlatformSocket &&acceptsocket, std::function<void(Socket)> &&accepthandler)
    {
        Context_->RegisterListener(std::make_shared<Listener>(*Context_, std::move(acceptsocket), std::move(accepthandler)));
        return shared_from_this();
    }
} // namespace NET
} // namespace SL
