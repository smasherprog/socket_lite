#include "ClientContext.h"
#include "ListenContext.h"
#include "Socket_Lite.h"
#include <assert.h>

namespace SL {
namespace NET {
    struct ContextInfo {
        ThreadCount ThreadCount_;
        PortNumber port;
        NetworkProtocol protocol;
    };
    class Client_Configuration : public IClient_Configuration {
      public:
        std::shared_ptr<ContextInfo> ContextInfo_;
        std::shared_ptr<ClientContext> ClientContext_;
        Client_Configuration(const std::shared_ptr<ContextInfo> &context) : ContextInfo_(context) {}
        virtual ~Client_Configuration() {}

        virtual std::shared_ptr<IClient_Configuration> onConnection(const std::function<void(const std::shared_ptr<ISocket> &)> &handle)
        {
            assert(!ClientContext_->onConnection);
            ClientContext_->onConnection = handle;
            return std::make_shared<Client_Configuration>(ContextInfo_);
        }

        virtual std::shared_ptr<IClient_Configuration> onMessage(const std::function<void(const std::shared_ptr<ISocket> &, const Message &)> &handle)
        {
            assert(!ClientContext_->onMessage);
            ClientContext_->onMessage = handle;
            return std::make_shared<Client_Configuration>(ContextInfo_);
        }
        virtual std::shared_ptr<IClient_Configuration> onDisconnection(const std::function<void(const std::shared_ptr<ISocket> &)> &handle)
        {
            assert(!ClientContext_->onDisconnection);
            ClientContext_->onDisconnection = handle;
            return std::make_shared<Client_Configuration>(ContextInfo_);
        }

        virtual std::shared_ptr<IContext> connect(const std::string &host, PortNumber port) { return ClientContext_; }
    };
    class Listener_Configuration : public IListener_Configuration {
      public:
        std::shared_ptr<ListenContext> ListenContext_;
        ThreadCount ThreadCount_;
        Listener_Configuration(const std::shared_ptr<ListenContext> &context) : ListenContext_(context) {}
        virtual ~Listener_Configuration() {}

        virtual std::shared_ptr<IListener_Configuration> onConnection(const std::function<void(const std::shared_ptr<ISocket> &)> &handle)
        {
            assert(!ListenContext_->onConnection);
            ListenContext_->onConnection = handle;
            return std::make_shared<Listener_Configuration>(ListenContext_);
        }
        virtual std::shared_ptr<IListener_Configuration>
        onMessage(const std::function<void(const std::shared_ptr<ISocket> &, const Message &)> &handle)
        {
            assert(!ListenContext_->onMessage);
            ListenContext_->onMessage = handle;
            return std::make_shared<Listener_Configuration>(ListenContext_);
        }
        virtual std::shared_ptr<IListener_Configuration> onDisconnection(const std::function<void(const std::shared_ptr<ISocket> &)> &handle)
        {
            assert(!ListenContext_->onDisconnection);
            ListenContext_->onDisconnection = handle;
            return std::make_shared<Listener_Configuration>(ListenContext_);
        }
        virtual std::shared_ptr<IContext> listen()
        {
            ListenContext_->run(ThreadCount_.value);
            return ListenContext_;
        }
    };

    class Context_Configuration : public IContext_Configuration {
      public:
        std::shared_ptr<ContextInfo> ContextInfo_;
        Context_Configuration(const std::shared_ptr<ContextInfo> &context) : ContextInfo_(context) {}
        virtual ~Context_Configuration() {}
        virtual std::shared_ptr<IListener_Configuration> CreateListener(PortNumber port, NetworkProtocol protocol)
        {
            auto ret = std::make_shared<Listener_Configuration>(std::make_shared<ListenContext>(port, protocol));
            ret->ThreadCount_ = ContextInfo_->ThreadCount_;
            return ret;
        }
        virtual std::shared_ptr<IClient_Configuration> CreateClient() { return std::make_shared<Client_Configuration>(ContextInfo_); }
    };

    std::shared_ptr<IContext_Configuration> CreateContext(ThreadCount threadcount)
    {
        auto ret = std::make_shared<Context_Configuration>(std::make_shared<ContextInfo>());
        ret->ContextInfo_->ThreadCount_ = threadcount;
        return ret;
    }
} // namespace NET
} // namespace SL