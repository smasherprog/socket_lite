#include "Context.h"
#include "Listener.h"
#include "Socket.h"
#include <chrono>

using namespace std::chrono_literals;
namespace SL {
namespace NET {
    std::shared_ptr<IContext> CreateContext() { return std::make_shared<Context>(); }
    Context::Context()
    {
        PendingIO = 0;

        if (!ConnectEx_) {
            auto handle = WSASocketW(AF_INET, SOCK_STREAM, IPPROTO_IP, NULL, 0, WSA_FLAG_OVERLAPPED);
            GUID guid = WSAID_CONNECTEX;
            DWORD bytes = 0;
            WSAIoctl(handle, SIO_GET_EXTENSION_FUNCTION_POINTER, &guid, sizeof(guid), &ConnectEx_, sizeof(ConnectEx_), &bytes, NULL, NULL);

            closesocket(handle);
        }
    }

    Context::~Context()
    {
        while (PendingIO > 0) {
            std::this_thread::sleep_for(5ms);
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

    void Context::run(ThreadCount threadcount)
    {

        Threads.reserve(threadcount.value);
        for (auto i = 0; i < threadcount.value; i++) {
            Threads.push_back(std::thread([&] {

                while (true) {
                    DWORD numberofbytestransfered = 0;
                    Win_IO_Context *overlapped = nullptr;
                    void *completionkey = nullptr;

                    auto bSuccess = GetQueuedCompletionStatus(iocp.handle, &numberofbytestransfered, (PDWORD_PTR)&completionkey,
                                                              (LPOVERLAPPED *)&overlapped, INFINITE) == TRUE;

                    if (!overlapped) {
                        return;
                    }
                    switch (overlapped->IOOperation) {
                    case IO_OPERATION::IoInitConnect:
                        static_cast<Socket *>(completionkey)->init_connect(bSuccess, static_cast<Win_IO_Connect_Context *>(overlapped));
                        break;
                    case IO_OPERATION::IoConnect:
                        static_cast<Socket *>(completionkey)->continue_connect(bSuccess, static_cast<Win_IO_Connect_Context *>(overlapped));
                        break;
                    case IO_OPERATION::IoStartAccept:
                        static_cast<Listener *>(completionkey)->start_accept(bSuccess, static_cast<Win_IO_Accept_Context *>(overlapped));
                        break;
                    case IO_OPERATION::IoAccept:
                        static_cast<Listener *>(completionkey)->handle_accept(bSuccess, static_cast<Win_IO_Accept_Context *>(overlapped));
                        break;
                    case IO_OPERATION::IoRead:
                        static_cast<Socket *>(completionkey)->increment_readbytes(numberofbytestransfered);
                        if (numberofbytestransfered == 0 && static_cast<Socket *>(completionkey)->ReadContext.bufferlen != 0 && bSuccess) {
                            bSuccess = WSAGetLastError() == WSA_IO_PENDING;
                        }
                        static_cast<Socket *>(completionkey)->continue_read(bSuccess);
                        break;
                    case IO_OPERATION::IoWrite:
                        static_cast<Socket *>(completionkey)->increment_writebytes(numberofbytestransfered);
                        if (numberofbytestransfered == 0 && static_cast<Socket *>(completionkey)->WriteContext.bufferlen != 0 && bSuccess) {
                            bSuccess = WSAGetLastError() == WSA_IO_PENDING;
                        }
                        static_cast<Socket *>(completionkey)->continue_write(bSuccess);
                        break;
                    default:
                        break;
                    }
                    if (--PendingIO <= 0) {
                        return;
                    }
                }
            }));
        }
    }
} // namespace NET
} // namespace SL