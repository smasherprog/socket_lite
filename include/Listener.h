#pragma once
#include "Socket_Lite.h"
#include <memory>
namespace SL {
namespace NET {
    class Socket;
    class Context;
    class Listener final : public IListener {
      public:
        Context *Context_;
        std::shared_ptr<Socket> ListenSocket;
        SL::NET::sockaddr ListenSocketAddr;

        Listener(Context *context, std::shared_ptr<Socket> &&socket, const sockaddr &addr);
        virtual ~Listener();
        virtual void close() override;
        virtual void accept(const std::function<void(StatusCode, Socket &&)> &&handler) override;

        static void start_accept(bool success, Win_IO_Accept_Context *context, Context *iocontext);
        static void handle_accept(bool success, Win_IO_Accept_Context *context, Context *iocontext);
    };

} // namespace NET
} // namespace SL
