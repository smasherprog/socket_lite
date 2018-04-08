
#include "Socket_Lite.h"
#include "defs.h"
#include <assert.h>
#if !_WIN32 
#include <sys/socket.h>
#include <sys/types.h>
#endif
namespace SL {
    namespace NET {

        Acceptor::Acceptor(PortNumber port, AddressFamily family, SL::NET::StatusCode &ec) : Family(family)
        {
            ec = SL::NET::StatusCode::SC_CLOSED;
            AcceptSocket = INVALID_SOCKET;
            auto[code, addresses] = SL::NET::getaddrinfo(nullptr, port, family);
            if (code != SL::NET::StatusCode::SC_SUCCESS) {
                ec = code;
            }
            else {
                for (auto &address : addresses) {
                    auto handle = INTERNAL::Socket(family);
                    if (::bind(handle, (::sockaddr *)address.get_SocketAddr(), address.get_SocketAddrLen()) == SOCKET_ERROR) {
                        ec = TranslateError();
                        CloseSocket(handle);
                    }
                    else {
                        if (::listen(handle, 5) == SOCKET_ERROR) {
                            ec = TranslateError();
                            CloseSocket(handle);
                        }
                        else {
                            INTERNAL::setsockopt_factory_impl<SL::NET::SocketOptions::O_REUSEADDR>::setsockopt_(handle, SL::NET::SockOptStatus::ENABLED);
                            ec = StatusCode::SC_SUCCESS;
                            AcceptSocket = handle;
                            return;
                        }
                    }
                }
            }
        }
        Acceptor::~Acceptor() { close(); }
        Acceptor::Acceptor(Acceptor&& a) : AcceptSocket(std::move(a.AcceptSocket)), Family(std::move(a.Family)), handler(std::move(a.handler)) {}
        void Acceptor::close() { CloseSocket(AcceptSocket); }

    } // namespace NET
} // namespace SL