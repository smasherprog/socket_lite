#include "Context.h"
#include "Listener.h"
#include "Socket.h"
#include <chrono>

using namespace std::chrono_literals;
namespace SL {
namespace NET {
    std::shared_ptr<IContext> CreateContext() { return std::make_shared<Context>(); }
    Context::Context() { PendingIO = 0; }

    Context::~Context()
    {
        KeepRunning = false;
        while (PendingIO != 0) {
            std::this_thread::sleep_for(1ms);
        }
        for (size_t i = 0; i < Threads.size(); i++) {
            PostQueuedCompletionStatus(iocp.handle, 0, (DWORD)NULL, NULL);
        }

        for (auto &t : Threads) {

            if (t.joinable()) {
                // destroying myself
                if (t.get_id() == std::this_thread::get_id()) {
                    t.detach();
                }
                else {
                    t.join();
                }
            }
        }
    }
    std::shared_ptr<ISocket> Context::CreateSocket() { return std::make_shared<Socket>(this); }
    std::shared_ptr<IListener> Context::CreateListener(std::shared_ptr<ISocket> &&listensocket)
    {
        auto addr = listensocket->getsockname();
        if (!addr.has_value()) {
            return std::shared_ptr<IListener>();
        }
        auto listener = std::make_shared<Listener>(this, std::forward<std::shared_ptr<ISocket>>(listensocket), addr.value());

        if (!Socket::UpdateIOCP(listener->ListenSocket->get_handle(), &iocp.handle, listener->ListenSocket.get())) {
            return std::shared_ptr<IListener>();
        }
        return listener;
    }
    void Context::handleaccept(bool success, Win_IO_Accept_Context *overlapped)
    {
        auto sock(std::move(overlapped->Socket_));
        auto handle(std::move(overlapped->completionhandler));
        overlapped->clear();
        if (!success || (success && !Socket::UpdateIOCP(sock->get_handle(), &iocp.handle, sock.get()))) {
            sock.reset();
            handle(TranslateError(), sock);
        }
        else {
            handle(StatusCode::SC_SUCCESS, sock);
        }
    }
    void Context::handleconnect(bool success, Socket *completionkey, Win_IO_RW_Context *overlapped)
    {
        completionkey->continue_connect(success ? StatusCode::SC_SUCCESS : TranslateError(), overlapped);
    }
    void Context::handlerecv(bool success, Socket *completionkey, Win_IO_RW_Context *overlapped, DWORD trasnferedbytes)
    {
        overlapped->transfered_bytes += trasnferedbytes;
        completionkey->continue_read(success, overlapped);
    }
    void Context::handlewrite(bool success, Socket *completionkey, Win_IO_RW_Context *overlapped, DWORD trasnferedbytes)
    {
        overlapped->transfered_bytes += trasnferedbytes;
        completionkey->continue_write(success, overlapped);
    }

    void Context::run(ThreadCount threadcount)
    {

        Threads.reserve(threadcount.value);
        for (auto i = 0; i < threadcount.value; i++) {
            Threads.push_back(std::thread([&] {

                while (true) {
                    DWORD numberofbytestransfered = 0;
                    Win_IO_Context *overlapped = nullptr;
                    Socket *completionkey = nullptr;

                    auto bSuccess = GetQueuedCompletionStatus(iocp.handle, &numberofbytestransfered, (PDWORD_PTR)&completionkey,
                                                              (LPOVERLAPPED *)&overlapped, INFINITE) == TRUE;

                    if (!overlapped) {
                        std::cout << "run" << std::endl;
                        return;
                    }
                    switch (overlapped->IOOperation) {
                    case IO_OPERATION::IoConnect:
                        handleconnect(bSuccess, completionkey, static_cast<Win_IO_RW_Context *>(overlapped));
                        break;
                    case IO_OPERATION::IoAccept:
                        handleaccept(bSuccess, static_cast<Win_IO_Accept_Context *>(overlapped));
                        break;
                    case IO_OPERATION::IoRead:
                        handlerecv(bSuccess, completionkey, static_cast<Win_IO_RW_Context *>(overlapped), numberofbytestransfered);
                        break;
                    case IO_OPERATION::IoWrite:
                        handlewrite(bSuccess, completionkey, static_cast<Win_IO_RW_Context *>(overlapped), numberofbytestransfered);
                        break;
                    default:
                        break;
                    }
                    if (--PendingIO == 0) {
                        return;
                    }
                }
            }));
        }
    }
} // namespace NET
} // namespace SL