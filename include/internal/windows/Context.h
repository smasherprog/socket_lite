#pragma once
#include "Socket_Lite.h"
#include "Structures.h"

namespace SL {
namespace NET {
    class Context : public IContext {
      public:
        WSARAII wsa;
        IOCP iocp;

        Context();
        virtual ~Context();

        std::function<void(const std::shared_ptr<ISocket> &)> onConnection;
        std::function<void(const std::shared_ptr<ISocket> &)> onDisconnection;
    };

} // namespace NET
} // namespace SL
