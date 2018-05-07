
#include "Socket_Lite.h"
#include "defs.h"
#include <assert.h>
#if !_WIN32
#include <netinet/ip.h>
#include <sys/epoll.h>
#endif
namespace SL {
namespace NET {

    Listener::Listener(Context &context, Acceptor &&acceptor) : Context_(context), Acceptor_(std::move(acceptor)) { Keepgoing = false; }
    Listener::~Listener()
    {
        stop();
        if (Runner.joinable()) {
            Runner.join();
        }
    }

    void Listener::stop()
    {
        Keepgoing = false;
        [[maybe_unused]] auto ret = Acceptor_.AcceptSocket.close();
        if (Runner.joinable()) {
            if (Runner.get_id() != std::this_thread::get_id()) {
                Runner.detach();
            }
            else {
                Runner.join();
            }
        }
    }
    bool Listener::isStopped() const { return Keepgoing; }
    void Listener::start()
    {
        assert(!Keepgoing);
        Keepgoing = true;
        Runner = std::thread([&]() {
            while (Keepgoing) {
                auto handle = ::accept(Acceptor_.AcceptSocket.Handle().value, NULL, NULL);
                if (handle != INVALID_SOCKET) {
                    PlatformSocket s(handle);
                    [[maybe_unused]] auto ret = s.setsockopt(BLOCKINGTag{}, SL::NET::Blocking_Options::NON_BLOCKING);

#if _WIN32
                    if (CreateIoCompletionPort((HANDLE)handle, Context_.ContextImpl_.IOCPHandle, NULL, NULL) == NULL) {
                        continue; // this shouldnt happen but what ever
                    }
#else
                    epoll_event ev = {0};
                    ev.events = EPOLLONESHOT;
                    if (epoll_ctl(Context_.ContextImpl_.IOCPHandle, EPOLL_CTL_ADD, handle, &ev) == -1) {
                        continue; // this shouldnt happen but what ever
                    }
#endif

                    Acceptor_.AcceptHandler(Socket(Context_, std::move(s)));
                }
            }
            Acceptor_.AcceptHandler = nullptr;
        });
    }
} // namespace NET
} // namespace SL
