#include "IO_Context.h"
#include "Socket.h"

std::shared_ptr<SL::NET::IIO_Context> SL::NET::CreateIO_Context(Address_Family fam) { return std::make_shared<IO_Context>(); }
SL::NET::IO_Context::IO_Context() {}

SL::NET::IO_Context::~IO_Context() {}

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

                if (lpOverlapped->IOOperation != IO_OPERATION::IoAccept &&
                    (bSuccess == FALSE || (bSuccess == TRUE && 0 == numberofbytestransfered))) {
                    // dropped connection
                    closeclient(completionkey, lpOverlapped);
                    async_accept(); // start waiting for a new connection
                    continue;
                }
                switch (lpOverlapped->IOOperation) {
                case IO_OPERATION::IoConnect:

                    if (setsockopt(completionkey->get_handle(), SOL_SOCKET, SO_UPDATE_CONNECT_CONTEXT, 0, 0) == SOCKET_ERROR) {
                        std::cerr << "Error setsockopt SO_UPDATE_CONNECT_CONTEXT Code: " << WSAGetLastError() << std::endl;
                        completionkey->ReadContext.IO_Context.completionhandler(-1);
                        return;
                    }
                    completionkey->ReadContext.IO_Context.completionhandler(bSuccess ? 0 : -1);
                    break;
                case IO_OPERATION::IoAccept:

                    if (setsockopt(lpOverlapped->IO_Context.Socket_->handle, SOL_SOCKET, SO_UPDATE_ACCEPT_CONTEXT, (char *)&ListenSocket,
                                   sizeof(ListenSocket)) == SOCKET_ERROR) {
                        std::cerr << "Error setsockopt SO_UPDATE_ACCEPT_CONTEXT Code: " << WSAGetLastError() << std::endl;
                        return;
                    }
                    if (auto ctx = CreateIoCompletionPort((HANDLE)lpOverlapped->IO_Context.Socket_->handle, iocp.handle, (DWORD_PTR)completionkey, 0);
                        ctx == NULL) {
                        std::cerr << "Error setsockopt SO_UPDATE_ACCEPT_CONTEXT Code: " << WSAGetLastError() << std::endl;
                        return;
                    }
                    onConnection(std::move(lpOverlapped->IO_Context.Socket_));
                    async_accept(); // start waiting for a new connection
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
