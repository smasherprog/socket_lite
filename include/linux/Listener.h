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
        Win_IO_Accept_Context Win_IO_Accept_Context_;
        std::atomic<size_t> &PendingIO;

        Listener(std::atomic<size_t> &counter, std::shared_ptr<ISocket> &&socket);
        virtual ~Listener();
        virtual void close();
        virtual void async_accept(std::shared_ptr<ISocket> &socket, const std::function<void(bool)> &&handler);
    };

} // namespace NET
} // namespace SL