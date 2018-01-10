#include "IO_Context.h"
#include "Listener.h"
#include "Socket.h"

std::shared_ptr<SL::NET::IIO_Context> SL::NET::CreateIO_Context() { return std::make_shared<IO_Context>(); }
SL::NET::IO_Context::IO_Context() {}

SL::NET::IO_Context::~IO_Context()
{
    Listener_.reset();
    KeepRunning = false;
    if (CancelIoEx(iocp.handle, NULL) == FALSE) {
        auto errocode = GetLastError();
        if (errocode != ERROR_NOT_FOUND) {
            std::cerr << "Error CancelIoEx Code: " << errocode << std::endl;
        }
    }
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
    // auto head = Clients;
    // while (head) {
    //    auto tmp = head;
    //    if (tmp->ReadContext) {
    //        tmp->read_complete(-1, tmp->ReadContext.get());
    //    }
    //    tmp->write_complete(-1, nullptr);
    //    head = pop_front(&Clients);
    //}
}

void SL::NET::IO_Context::run(ThreadCount threadcount)
{

    Threads.reserve(threadcount.value);
    for (auto i = 0; i < threadcount.value; i++) {
        Threads.push_back(std::thread([&] {

            while (KeepRunning) {
                DWORD numberofbytestransfered = 0;
                Win_IO_Context *lpOverlapped = nullptr;
                Socket *completionkey = nullptr;

                auto bSuccess = GetQueuedCompletionStatus(iocp.handle, &numberofbytestransfered, (PDWORD_PTR)&completionkey,
                                                          (LPOVERLAPPED *)&lpOverlapped, INFINITE) == TRUE;
                if (!KeepRunning) {
                    return;
                }
                switch (lpOverlapped->IOOperation) {
                case IO_OPERATION::IoConnect:
                    if (bSuccess) {
                        if (setsockopt(completionkey->get_handle(), SOL_SOCKET, SO_UPDATE_CONNECT_CONTEXT, 0, 0) == SOCKET_ERROR) {
                            std::cerr << "Error setsockopt SO_UPDATE_CONNECT_CONTEXT Code: " << WSAGetLastError() << std::endl;
                            completionkey->ReadContext.IO_Context.completionhandler(-1);
                        }
                        completionkey->ReadContext.IO_Context.completionhandler(bSuccess ? 0 : -1);
                    }
                    else {
                        completionkey->ReadContext.IO_Context.completionhandler(-1);
                    }
                    break;
                case IO_OPERATION::IoAccept:

                    if (setsockopt(completionkey->get_handle(), SOL_SOCKET, SO_UPDATE_ACCEPT_CONTEXT, (char *)&Listener_->get_handle(),
                                   sizeof(Listener_->get_handle())) == SOCKET_ERROR) {
                        std::cerr << "Error setsockopt SO_UPDATE_ACCEPT_CONTEXT Code: " << WSAGetLastError() << std::endl;
                        return;
                    }
                    Socket::UpdateIOCP() if (auto ctx = CreateIoCompletionPort((HANDLE)lpOverlapped->IO_Context.Socket_->handle, iocp.handle,
                                                                               (DWORD_PTR)completionkey, 0);
                                             ctx == NULL)
                    {
                        std::cerr << "Error setsockopt SO_UPDATE_ACCEPT_CONTEXT Code: " << WSAGetLastError() << std::endl;
                        return;
                    }
                    onConnection(std::move(lpOverlapped->IO_Context.Socket_));
                    break;
                case IO_OPERATION::IoRead:
                    lpOverlapped->IO_Context.transfered_bytes += numberofbytestransfered;
                    completionkey->continue_read(lpOverlapped);
                    if (completionkey->handle == INVALID_SOCKET) {
                        closeclient(completionkey, lpOverlapped);
                    }
                    break;
                case IO_OPERATION::IoWrite:
                    lpOverlapped->IO_Context.transfered_bytes += numberofbytestransfered;
                    completionkey->continue_write(static_cast<Win_IO_Context_List *>(lpOverlapped));
                    if (completionkey->handle == INVALID_SOCKET) {
                        closeclient(completionkey, lpOverlapped);
                    }
                    break;
                default:
                    break;
                }
            }
        }));
    }
}
