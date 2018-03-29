#include "Context.h"
#include "Listener.h"
#include "Socket.h"
#include <Ws2tcpip.h>
#include <algorithm>
#include <chrono>

using namespace std::chrono_literals;
namespace SL {
namespace NET {
    std::shared_ptr<IContext> CreateContext(ThreadCount threadcount) { return std::make_shared<Context>(threadcount); }
    Context::Context(ThreadCount threadcount) : ThreadCount_(threadcount)
    {
        if (WSAStartup(0x202, &wsaData) != 0) {
            abort();
        }
        IOCPHandle = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, threadcount.value);

        PendingIO = 0;

        auto handle = WSASocketW(AF_INET, SOCK_STREAM, IPPROTO_IP, NULL, 0, WSA_FLAG_OVERLAPPED);
        GUID guid = WSAID_CONNECTEX;
        DWORD bytes = 0;
        WSAIoctl(handle, SIO_GET_EXTENSION_FUNCTION_POINTER, &guid, sizeof(guid), &ConnectEx_, sizeof(ConnectEx_), &bytes, NULL, NULL);

        GUID acceptex_guid = WSAID_ACCEPTEX;
        bytes = 0;
        WSAIoctl(handle, SIO_GET_EXTENSION_FUNCTION_POINTER, &acceptex_guid, sizeof(acceptex_guid), &AcceptEx_, sizeof(AcceptEx_), &bytes, NULL,
                 NULL);

        closesocket(handle);
        if (IOCPHandle == NULL || AcceptEx_ == nullptr || ConnectEx_ == nullptr) {
            abort();
        }
    }

    Context::~Context()
    {
        while (PendingIO > 0) {
            std::this_thread::sleep_for(5ms);
        }

        PostQueuedCompletionStatus(IOCPHandle, 0, (DWORD)NULL, NULL);

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
        CloseHandle(IOCPHandle);
        WSACleanup();
    }

    std::shared_ptr<ISocket> Context::CreateSocket() { return std::make_shared<Socket>(this); }
    std::shared_ptr<IListener> Context::CreateListener(std::shared_ptr<ISocket> &&listensocket)
    {
        auto addr = listensocket->getsockname();
        if (!addr.has_value()) {
            return std::shared_ptr<IListener>();
        }
        auto listener = std::make_shared<Listener>(this, std::forward<std::shared_ptr<ISocket>>(listensocket), addr.value());

        if (CreateIoCompletionPort((HANDLE)listener->ListenSocket->get_handle(), IOCPHandle, NULL, NULL) == NULL) {
            return std::shared_ptr<IListener>();
        }
        return listener;
    }
    void Context::run()
    {
        Threads.reserve(ThreadCount_.value);
        for (auto i = 0; i < ThreadCount_.value; i++) {
            Threads.push_back(std::thread([&] {
                while (true) {
                    DWORD numberofbytestransfered = 0;
                    Win_IO_Context *overlapped = nullptr;
                    void *completionkey = nullptr;

                    auto bSuccess = GetQueuedCompletionStatus(IOCPHandle, &numberofbytestransfered, (PDWORD_PTR)&completionkey,
                                                              (LPOVERLAPPED *)&overlapped, INFINITE) == TRUE;

                    if (PendingIO <= 0) {
                        PostQueuedCompletionStatus(IOCPHandle, 0, (DWORD)NULL, NULL);
                        return;
                    }
                    switch (overlapped->IOOperation) {

                    case IO_OPERATION::IoConnect:
                        Socket::continue_connect(bSuccess, static_cast<Win_IO_RW_Context *>(overlapped));
                        break;
                    case IO_OPERATION::IoStartAccept:
                        Listener::start_accept(bSuccess, static_cast<Win_IO_Accept_Context *>(overlapped));
                        break;
                    case IO_OPERATION::IoAccept:
                        Listener::handle_accept(bSuccess, static_cast<Win_IO_Accept_Context *>(overlapped));
                        break;
                    case IO_OPERATION::IoRead:
                    case IO_OPERATION::IoWrite:
                        static_cast<Win_IO_RW_Context *>(overlapped)->transfered_bytes += numberofbytestransfered;
                        if (numberofbytestransfered == 0 && static_cast<Win_IO_RW_Context *>(overlapped)->bufferlen != 0 && bSuccess) {
                            bSuccess = WSAGetLastError() == WSA_IO_PENDING;
                        }
                        Socket::continue_io(bSuccess, static_cast<Win_IO_RW_Context *>(overlapped));
                        break;
                    default:
                        break;
                    }
                    if (--PendingIO <= 0) {
                        PostQueuedCompletionStatus(IOCPHandle, 0, (DWORD)NULL, NULL);
                        return;
                    }
                }
            }));
        }
    }
} // namespace NET
} // namespace SL