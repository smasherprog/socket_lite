#pragma once

#include "defs.h"
#include "memorypool.h"
#include "sockfuncs.h"
#include <algorithm>
#include <thread>

namespace SL {
namespace NET {
    class Context;
    class SOCKET_LITE_EXTERN Socket {
        PlatformSocket handle;
        Context *Context_;

      public:
        Socket();
        Socket(const Socket &);
        Socket &operator=(const Socket &);
        Socket(Socket &&);
        Socket &operator=(Socket &&);
        Socket(Context *context);
        Socket(Context *context, AddressFamily family);

        ~Socket();
        template <SocketOptions SO> auto getsockopt() { return INTERNAL::getsockopt_factory_impl<SO>::getsockopt_(handle); }
        template <SocketOptions SO, typename... Args> auto setsockopt(Args &&... args)
        {
            return INTERNAL::setsockopt_factory_impl<SO>::setsockopt_(handle, std::forward<Args>(args)...);
        }
        std::optional<SL::NET::sockaddr> getpeername() const;
        std::optional<SL::NET::sockaddr> getsockname() const;
        StatusCode bind(sockaddr addr);
        StatusCode listen(int backlog) const;

        static bool UpdateIOCP(SOCKET socket, HANDLE *iocp);
        static void connect(Context *iocontext, sockaddr &address, const std::function<void(StatusCode, Socket &&)> &&);
        void recv(size_t buffer_size, unsigned char *buffer, std::function<void(StatusCode, size_t)> &&handler);
        void send(size_t buffer_size, unsigned char *buffer, std::function<void(StatusCode, size_t)> &&handler);

        static void continue_io(bool success, Win_IO_RW_Context *context, Context *iocontext);
        static void init_connect(bool success, Win_IO_Connect_Context *context, Context *iocontext);
        static void continue_connect(bool success, Win_IO_Connect_Context *context, Context *iocontext);

        PlatformSocket get_handle() const { return handle; }
        void set_handle(PlatformSocket h);
        void close();
    };
    class SOCKET_LITE_EXTERN IListener {
      public:
        virtual ~IListener() {}
        virtual void close() = 0;
        virtual void accept(const std::function<void(StatusCode, Socket &&)> &&handler) = 0;
    };
    class Listener;
    class SOCKET_LITE_EXTERN Context {
        std::vector<std::thread> Threads;
        ThreadCount ThreadCount_;
        IOCP iocp;
        std::atomic<int> PendingIO;
#if WIN32
        WSARAII wsa;
        LPFN_CONNECTEX ConnectEx_;
        LPFN_ACCEPTEX AcceptEx_ = nullptr;
#else
        int EventWakeFd = -1;
#endif
        MemoryPool<Win_IO_RW_Context *, Win_IO_RW_ContextCRUD> Win_IO_RW_ContextBuffer;
        MemoryPool<RW_CompletionHandler *, RW_CompletionHandlerCRUD> RW_CompletionHandlerBuffer;
        MemoryPool<Win_IO_Accept_Context *, Win_IO_Accept_ContextCRUD> Win_IO_Accept_ContextBuffer;
        MemoryPool<Win_IO_Connect_Context *, Win_IO_Connect_ContextCRUD> Win_IO_Connect_ContextBuffer;

      public:
        Context(ThreadCount threadcount);
        ~Context();
        void run();
        std::shared_ptr<IListener> CreateListener(std::shared_ptr<Socket> &&listensocket); // listener steals the socket
        bool inWorkerThread() const
        {
            return std::any_of(std::begin(Threads), std::end(Threads), [](const std::thread &t) { return t.get_id() == std::this_thread::get_id(); });
        }
        friend Socket;
        friend Listener;
    };

} // namespace NET
} // namespace SL
