#pragma once
#include "Socket_Lite.h"
#include "Structures.h"
#include <memory>
namespace SL {
namespace NET {
    class Socket;
    class Context;
    class Listener final : public IListener {
      public:
#ifdef WIN32
        LPFN_ACCEPTEX AcceptEx_ = nullptr;
        char Buffer[(sizeof(SOCKADDR_STORAGE) + 16) * 2];
        void start_accept(bool success, Win_IO_Accept_Context *context);
        void handle_accept(bool success, Win_IO_Accept_Context *context);
#endif //  WIN32

        std::atomic<int> &PendingIO;
        IOCP &iocp;
        std::shared_ptr<Socket> ListenSocket;
        SL::NET::sockaddr ListenSocketAddr;

        Listener(Context *context, std::shared_ptr<ISocket> &&socket, const sockaddr &addr);
        virtual ~Listener();
        virtual void close() override;
        virtual void accept(const std::function<void(StatusCode, const std::shared_ptr<ISocket> &)> &&handler) override;
    };

} // namespace NET
} // namespace SL
