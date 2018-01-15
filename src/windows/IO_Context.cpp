#include "IO_Context.h"
#include "Listener.h"
#include "Socket.h"
#include <chrono>

using namespace std::chrono_literals;

std::shared_ptr<SL::NET::IIO_Context> SL::NET::CreateIO_Context() { return std::make_shared<IO_Context>(); }
SL::NET::IO_Context::IO_Context() { PendingIO = 0; }

SL::NET::IO_Context::~IO_Context()
{
    KeepRunning = false;
    Listener_.reset(); // release the listener
    while (PendingIO != 0) {
        std::this_thread::sleep_for(1ms);
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
void SL::NET::IO_Context::handleaccept(Win_IO_Accept_Context *overlapped)
{
    if (::setsockopt(overlapped->Socket_->get_handle(), SOL_SOCKET, SO_UPDATE_ACCEPT_CONTEXT, (char *)&overlapped->ListenSocket,
                     sizeof(overlapped->ListenSocket) == SOCKET_ERROR)) {
        std::cerr << "Error setsockopt SO_UPDATE_ACCEPT_CONTEXT Code: " << WSAGetLastError() << std::endl;
        //   overlapped->completionhandler(false);
        //  return;
    }
    if (!Socket::UpdateIOCP(overlapped->Socket_->get_handle(), &iocp.handle, overlapped->Socket_.get())) {
        std::cerr << "Error setsockopt SO_UPDATE_ACCEPT_CONTEXT Code: " << WSAGetLastError() << std::endl;
        overlapped->completionhandler(false);
    }
    else {
        overlapped->completionhandler(true);
    }
    overlapped->clear();
}
void SL::NET::IO_Context::handleconnect(bool success, Socket *completionkey, Win_IO_Connect_Context *overlapped)
{
    completionkey->continue_connect(success ? ConnectionAttemptStatus::SuccessfullConnect : ConnectionAttemptStatus::FailedConnect, overlapped);
}
void SL::NET::IO_Context::handlerecv(bool success, Socket *completionkey, Win_IO_RW_Context *overlapped, DWORD trasnferedbytes)
{
    overlapped->transfered_bytes += trasnferedbytes;
    completionkey->continue_read(overlapped);
}
void SL::NET::IO_Context::handlewrite(bool success, Socket *completionkey, Win_IO_RW_Context *overlapped, DWORD trasnferedbytes)
{
    overlapped->transfered_bytes += trasnferedbytes;
    completionkey->continue_write(overlapped);
}
void SL::NET::IO_Context::run(ThreadCount threadcount)
{

    Threads.reserve(threadcount.value);
    for (auto i = 0; i < threadcount.value; i++) {
        Threads.push_back(std::thread([&] {

            while (KeepRunning) {
                DWORD numberofbytestransfered = 0;
                Win_IO_Context *overlapped = nullptr;
                Socket *completionkey = nullptr;

                auto bSuccess = GetQueuedCompletionStatus(iocp.handle, &numberofbytestransfered, (PDWORD_PTR)&completionkey,
                                                          (LPOVERLAPPED *)&overlapped, INFINITE) == TRUE;
                auto newpending = --PendingIO;
                if (!KeepRunning && newpending == 0) {
                    return;
                }
                switch (overlapped->IOOperation) {
                case IO_OPERATION::IoConnect:
                    handleconnect(bSuccess, completionkey, static_cast<Win_IO_Connect_Context *>(overlapped));
                    break;
                case IO_OPERATION::IoAccept:
                    handleaccept(static_cast<Win_IO_Accept_Context *>(overlapped));
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
            }
        }));
    }
}
