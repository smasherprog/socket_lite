#pragma once

#include "Internal/spinlock.h"
#include "Socket_Lite.h"
#include <algorithm>
#include <atomic>
#include <functional> 
#include <memory> 
#include <vector>
#include "defs.h"

namespace SL {
namespace NET {

    class Listener;
    class Context {
        const ThreadCount ThreadCount_;
        std::vector<std::thread> Threads;
        std::vector<RW_Context> ReadContexts, WriteContexts; 
        std::vector<std::shared_ptr<Listener>> Listeners;
        bool KeepGoing_;
        std::atomic<int> PendingIO;
#if _WIN32
        HANDLE IOCPHandle;
        WSADATA wsaData;

      public:
        LPFN_CONNECTEX ConnectEx_;
        LPFN_ACCEPTEX AcceptEx_;
        HANDLE getIOHandle() const;
        void wakeup();
#else
        int EventWakeFd;
        int EventFd;
        int IOCPHandle;
        std::vector<int> ReadSockets, WriteSockets;
        spinlock ReadSocketLock, WriteSocketLock;

      public:
        int getIOHandle() const;
        void wakeup();
        void wakeupReadfd(int fd);
        void wakeupWritefd(int fd);

#endif

        Context(ThreadCount t);
        ~Context();
        void run();
        unsigned int getThreadCount() const { return ThreadCount_.value; }
        int IncrementPendingIO() { return PendingIO.fetch_add(1, std::memory_order_acquire) + 1; }
        int DecrementPendingIO() { return PendingIO.fetch_sub(1, std::memory_order_acquire) - 1; }
        int getPendingIO() const { return PendingIO.load(std::memory_order_relaxed); }
        void stop() { KeepGoing_ = false; }
        void RegisterListener(std::shared_ptr<Listener> l); 
        void RegisterSocket(const SocketHandle &);
        RW_Context &getWriteContext(const SocketHandle &socket);
        RW_Context &getReadContext(const SocketHandle &socket); 
        void DeregisterSocket(const SocketHandle &socket);
    };

} // namespace NET
} // namespace SL
