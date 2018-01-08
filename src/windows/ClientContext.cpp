#include "ClientContext.h"
#include "Socket.h"
#include <iostream>

namespace SL {
namespace NET {

    ClientContext::ClientContext() {}
    ClientContext::~ClientContext()
    {
        KeepRunning = false;

        for (size_t i = 0; i < Threads.size(); i++) {
            // Help threads get out of blocking - GetQueuedCompletionStatus()
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
        Socket_.reset();
    }
    bool ClientContext::async_connect(std::string host, PortNumber port)
    {
        Socket_ = std::make_shared<Socket>();
        ConnectIOContext.IOOperation = IO_OPERATION::IoConnect;
        if (auto socket = create_and_connect(host, port, &iocp.handle, Socket_.get(), (void *)&ConnectIOContext.Overlapped); socket.has_value()) {
            Socket_->handle = *socket;
            return true;
        }
        Socket_.reset();
        if (onConnection)
            onConnection(Socket_);
        return false;
    }
    void ClientContext::closeclient(IO_OPERATION op, Win_IO_Context *context)
    {
        if (op == IO_OPERATION::IoRead) {
            Socket_->read_complete(-1, context);
        }
        else if (op == IO_OPERATION::IoWrite) {
            Socket_->write_complete(-1, static_cast<Win_IO_Context_List *>(context));
        }
    }
    void ClientContext::run(ThreadCount threadcount)
    {

        Threads.reserve(threadcount.value);
        for (auto i = 0; i < threadcount.value; i++) {
            Threads.push_back(std::thread([&] {

                while (KeepRunning) {
                    DWORD numberofbytestransfered = 0;
                    Win_IO_Context *lpOverlapped = nullptr;
                    Socket *completionkey = nullptr;

                    auto bSuccess = GetQueuedCompletionStatus(iocp.handle, &numberofbytestransfered, (PDWORD_PTR)&completionkey,
                                                              (LPOVERLAPPED *)&lpOverlapped, 100);
                    if (!KeepRunning) {
                        return;
                    }
                    if (bSuccess == FALSE && lpOverlapped == NULL) {
                        if (GetLastError() == ERROR_ABANDONED_WAIT_0) {
                            return; // the iocp handle was closed!
                        }
                        continue; // timer ran out go back to top and try again
                    }

                    if (lpOverlapped->IOOperation != IO_OPERATION::IoConnect &&
                        (bSuccess == FALSE || (bSuccess == TRUE && 0 == numberofbytestransfered))) {
                        // dropped connection
                        closeclient(lpOverlapped->IOOperation, lpOverlapped);
                        continue;
                    }
                    switch (lpOverlapped->IOOperation) {
                    case IO_OPERATION::IoConnect:
                        if (setsockopt(Socket_->handle, SOL_SOCKET, SO_UPDATE_CONNECT_CONTEXT, (char *)&Socket_->handle, sizeof(Socket_->handle)) ==
                            SOCKET_ERROR) {
                            std::cerr << "Error setsockopt SO_UPDATE_CONNECT_CONTEXT Code: " << WSAGetLastError() << std::endl;
                            return;
                        }
                        onConnection(Socket_);
                        break;
                    case IO_OPERATION::IoRead:
                        lpOverlapped->IO_Context.transfered_bytes += numberofbytestransfered;
                        completionkey->continue_read(lpOverlapped);
                        if (completionkey->handle == INVALID_SOCKET) {
                            closeclient(lpOverlapped->IOOperation, lpOverlapped);
                        }
                        break;
                    case IO_OPERATION::IoWrite:
                        lpOverlapped->IO_Context.transfered_bytes += numberofbytestransfered;
                        completionkey->continue_write(static_cast<Win_IO_Context_List *>(lpOverlapped));
                        if (completionkey->handle == INVALID_SOCKET) {
                            closeclient(lpOverlapped->IOOperation, lpOverlapped);
                        }
                        break;
                    default:
                        break;
                    }
                }
            }));
        }
    }
} // namespace NET

} // namespace SL