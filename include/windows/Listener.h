#pragma once
#include "Socket_Lite.h"
#include "Structures.h"
#include <memory>
namespace SL {
namespace NET {
#define MAX_BUFF_SIZE 8192
    class Socket;
    class Listener final : public IListener {
      public:
        LPFN_ACCEPTEX AcceptEx_ = nullptr;
        char Buffer[MAX_BUFF_SIZE];
        std::shared_ptr<Socket> ListenSocket;

        std::atomic<size_t> &PendingIO;
        Listener(std::shared_ptr<ISocket> &&socket, std::atomic<size_t> &counter);
        virtual ~Listener();

        virtual void async_accept(std::shared_ptr<ISocket> &socket, const std::function<void(bool)> &&handler);
    };

} // namespace NET
} // namespace SL