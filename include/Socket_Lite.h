#pragma once

#include "defs.h"
#include "sockfuncs.h"

namespace SL {
namespace NET {

    class SOCKET_LITE_EXTERN ISocket {
      protected:
        PlatformSocket handle;

      public:
        ISocket();
        virtual ~ISocket() { close(); };
        template <SocketOptions SO> auto getsockopt() { return INTERNAL::getsockopt_factory_impl<SO>::getsockopt_(handle); }
        template <SocketOptions SO, typename... Args> auto setsockopt(Args &&... args)
        {
            return INTERNAL::setsockopt_factory_impl<SO>::setsockopt_(handle, std::forward<Args>(args)...);
        }
        std::optional<SL::NET::sockaddr> getpeername() const;
        std::optional<SL::NET::sockaddr> getsockname() const;
        StatusCode bind(sockaddr addr);
        StatusCode listen(int backlog) const;
        virtual void connect(SL::NET::sockaddr &address, const std::function<void(StatusCode)> &&) = 0;
        virtual void recv(size_t buffer_size, unsigned char *buffer, std::function<void(StatusCode, size_t)> &&handler) = 0;
        virtual void send(size_t buffer_size, unsigned char *buffer, std::function<void(StatusCode, size_t)> &&handler) = 0;
        virtual void close();
        PlatformSocket get_handle() const { return handle; }
        void set_handle(PlatformSocket h);
    };
    class SOCKET_LITE_EXTERN IListener {
      public:
        virtual ~IListener() {}
        virtual void close() = 0;
        virtual void accept(const std::function<void(StatusCode, const std::shared_ptr<ISocket> &)> &&handler) = 0;
    };
    class SOCKET_LITE_EXTERN IContext {
      public:
        virtual ~IContext() {}
        virtual void run() = 0;
        virtual std::shared_ptr<ISocket> CreateSocket() = 0;
        virtual std::shared_ptr<IListener> CreateListener(std::shared_ptr<ISocket> &&listensocket) = 0; // listener steals the socket
    };
    std::shared_ptr<IContext> SOCKET_LITE_EXTERN CreateContext(ThreadCount threadcount);

} // namespace NET
} // namespace SL
