#pragma once
#include "Context.h"
#include <string>
namespace SL {
namespace NET {
    class Acceptor {
        LPFN_ACCEPTEX AcceptEx_ = nullptr;
        Context &Context_;
        Socket SocketContext_;
        unsigned char AcceptBuffer[2 * (sizeof(SOCKADDR_STORAGE) + 16)];

      public:
        Acceptor(Context &context, unsigned short port);
        ~Acceptor();

        operator bool() const { return SocketContext_.operator bool(); }
        void close();
        void async_accept(const std::shared_ptr<Socket> &socket);
    };
} // namespace NET
} // namespace SL