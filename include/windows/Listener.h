#pragma once
#include "Socket_Lite.h"
#include "Structures.h"
#include <memory>
namespace SL {
namespace NET {
    class Socket;
    class Listener final : public IListener {
      public:
        LPFN_ACCEPTEX AcceptEx_ = nullptr;
        char Buffer[(sizeof(SOCKADDR_STORAGE) + 16) * 2];
        std::shared_ptr<Socket> ListenSocket;
        Context *Context_;
        Win_IO_Accept_Context Win_IO_Accept_Context_;
        sockaddr ListenSocketAddr;

        Listener(Context *context, std::shared_ptr<ISocket> &&socket, const sockaddr &addr);
        virtual ~Listener();
        virtual void close() override;
        virtual void async_accept(const std::function<void(const std::shared_ptr<ISocket> &)> &&handler) override;
    };

} // namespace NET
} // namespace SL