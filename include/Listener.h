#pragma once
#include "Context.h"
#include "PlatformSocket.h"
#include "Socket.h"

namespace SL::NET {

    template <class LISTENERCALLBACK> class Listener {
        static_assert(std::is_move_constructible<LISTENERCALLBACK>::value, "The listener callback must be moveable!"); 
        PlatformSocket ListenSocket;
        LISTENERCALLBACK AcceptHandler;
        std::thread Runner;
        bool Keepgoing;
        Context &ContextImpl_;
 
      public:
        Listener(Context &c, PlatformSocket &&acceptsocket, LISTENERCALLBACK &accepthandler)
            : ListenSocket(std::move(acceptsocket)), AcceptHandler(std::move(accepthandler)), ContextImpl_(c)
        {
            Keepgoing = true;
        }

        ~Listener() { stop(); }
        void start() { 
            assert(Keepgoing);
            Runner = std::thread([&]() {
                while (Keepgoing) {
#if _WIN32

                    auto handle = ::accept(ListenSocket.Handle().value, NULL, NULL);
                    if (handle != INVALID_SOCKET) {
                        PlatformSocket s(handle);
                        if (CreateIoCompletionPort((HANDLE)handle, ContextImpl_.getIOHandle(), handle, NULL) != NULL) {
                            [[maybe_unused]] auto ret = s.setsockopt(BLOCKINGTag{}, SL::NET::Blocking_Options::NON_BLOCKING);
                            AcceptHandler(Socket(ContextImpl_, std::move(s)));
                        }
                    }
#else

                    auto handle = ::accept4(ListenSocket.Handle().value, NULL, NULL, SOCK_NONBLOCK);
                    if (handle != INVALID_SOCKET) {
                        PlatformSocket s(handle);
                        epoll_event ev = {0};
                        ev.events = EPOLLONESHOT;
                        if (epoll_ctl(ContextImpl_.getIOHandle(), EPOLL_CTL_ADD, handle, &ev) == 0) {
                            AcceptHandler(Socket(ContextImpl_, std::move(s)));
                        }
                    }
#endif
                }
                auto acceptcb(std::move(AcceptHandler)); // make sure the handler is destroyed as early as possible
            });
        }
        void stop()
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

}  