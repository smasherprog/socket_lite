#pragma once
#include "Socket_Lite.h"
#include "Structures.h"
#include <atomic>
#include <thread>

namespace SL
{
namespace NET
{
class Listener;
class Context final : public IContext
{

    bool KeepRunning = true;
    std::vector<std::thread> Threads;

public:
    IOCP iocp;
    std::atomic<size_t> PendingIO;

    Context();
    ~Context();
    virtual void run(ThreadCount threadcount) override;
    virtual std::shared_ptr<ISocket> CreateSocket() override;
    virtual std::shared_ptr<IListener> CreateListener(std::shared_ptr<ISocket>&& listensocket) override;

    void handleaccept(int socket);

};
} // namespace NET
} // namespace SL
