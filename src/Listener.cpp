
#include "Socket_Lite.h"
#include "defs.h"
#include <assert.h>

namespace SL {
    namespace NET {

        Listener::Listener(Context &context, Acceptor&& acceptor)
            : Context_(context), Acceptor_(std::move(acceptor))
        {
            Keepgoing = true;
            Runner = std::thread([&]() {
                while (Keepgoing) {
                    auto handle = ::accept(Acceptor_.AcceptSocket, NULL, NULL);
                    if (handle != INVALID_SOCKET) {
                        Socket sock(Context_);
                        SocketGetter sg1(sock);
                        sg1.setSocket(handle);
                        INTERNAL::setsockopt_factory_impl<SL::NET::SocketOptions::O_BLOCKING>::setsockopt_(handle, SL::NET::Blocking_Options::NON_BLOCKING);

#if _WIN32
                        if (CreateIoCompletionPort((HANDLE)handle, sg1.getIOCPHandle(), NULL, NULL) == NULL) {
                            continue;// this shouldnt happen but what ever
                        }
#else
                        epoll_event ev = { 0 };
                        ev.events = EPOLLONESHOT;
                        if (epoll_ctl(sg1.getIOCPHandle(), EPOLL_CTL_ADD, i, &ev) == -1) {
                            continue;// this shouldnt happen but what ever
                        }
#endif

                        Acceptor_.handler(std::move(sock));
                        }
                    }

                Acceptor_.handler = nullptr;
                });
            }
        Listener::~Listener() {
            close();
            if (Runner.joinable()) {
                Runner.join();
            }
        }
        void Listener::close() {
            Keepgoing = false;
            Acceptor_.close();
        }

        } // namespace NET
    } // namespace SL