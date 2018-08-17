#pragma once
#include "AsyncPlatformSocket.h"
#include "Internal.h"

namespace SL::NET {

template <class SOCKETHANDLERTYPE> class AsyncAcceptor {

    AsyncPlatformSocket AcceptorSocket;
    Context &Context_;
    SOCKETHANDLERTYPE Handler;
    bool Keepgoing = true;
    std::thread Runner;

  public:
    AsyncAcceptor(AsyncPlatformSocket &&socket, SOCKETHANDLERTYPE &&callback) noexcept
        : Context_(socket.getContext()), AcceptorSocket(std::forward<AsyncPlatformSocket>(socket)),
          Handler(std::forward<SOCKETHANDLERTYPE>(callback)), Runner([&]() {
              [[maybe_unused]] auto setblocking = AcceptorSocket.setsockopt(BLOCKINGTag{}, SL::NET::Blocking_Options::BLOCKING);
              while (Keepgoing) {
#if _WIN32

                  SocketHandle handle = ::accept(AcceptorSocket.Handle().value, NULL, NULL);
                  if (handle.value != INVALID_SOCKET) {
                      AsyncPlatformSocket s(Context_, handle);
                      if (CreateIoCompletionPort((HANDLE)handle.value, Context_.getIOHandle(), handle.value, NULL) != NULL) {
                          [[maybe_unused]] auto ret = s.setsockopt(BLOCKINGTag{}, SL::NET::Blocking_Options::NON_BLOCKING);
                          Handler(std::move(s));
                      }
                  }
#else

                  SocketHandle handle = ::accept4(AcceptorSocket.Handle().value, NULL, NULL, SOCK_NONBLOCK);
                  if (handle.value != INVALID_SOCKET) {
                      AsyncPlatformSocket s(Context_, handle);
                      epoll_event ev = {0};
                      ev.events = EPOLLONESHOT;
                      if (epoll_ctl(ContextImpl_.getIOHandle(), EPOLL_CTL_ADD, handle.value, &ev) == 0) {
                          Handler(std::move(s));
                      }
                  }
#endif
              }
              auto acceptcb(std::move(Handler)); // make sure the handler is destroyed as early as possible
          })
    {
    }

    ~AsyncAcceptor()
    {
        Keepgoing = false;
#if _WIN32
        AcceptorSocket.close();
#else
        AcceptorSocket.shutdown(ShutDownOptions::SHUTDOWN_READ);
#endif

        if (Runner.joinable()) {
            Runner.join();
        }
    }
};
} // namespace SL::NET
