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
    while (PendingIO != 0) {
        std::this_thread::sleep_for(1ms);
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
}
void SL::NET::IO_Context::handleaccept(bool success, Win_IO_Accept_Context *overlapped)
{
    if (!success) {
        auto handle(std::move(overlapped->completionhandler));
        overlapped->clear();
        if (handle) {
            handle(false);
        }
        return;
    }

    if (!Socket::UpdateIOCP(overlapped->Socket_->get_handle(), &iocp.handle, overlapped->Socket_.get())) {
        std::cerr << "Error setsockopt SO_UPDATE_ACCEPT_CONTEXT Code: " << WSAGetLastError() << std::endl;
        auto handle(std::move(overlapped->completionhandler));
        overlapped->clear();
        handle(false);
    }
    else {
        auto handle(std::move(overlapped->completionhandler));
        overlapped->clear();
        handle(true);
    }
}
void SL::NET::IO_Context::handleconnect(bool success, Socket *completionkey, Win_IO_Connect_Context *overlapped)
{
    completionkey->continue_connect(success ? ConnectionAttemptStatus::SuccessfullConnect : ConnectionAttemptStatus::FailedConnect, overlapped);
}
void SL::NET::IO_Context::handlerecv(bool success, Socket *completionkey, Win_IO_RW_Context *overlapped, DWORD trasnferedbytes)
{
    if (trasnferedbytes == 0) {
        std::cout << "Dropped Connection " << (int)success << std::endl;
        success = false;
    }
    overlapped->transfered_bytes += trasnferedbytes;
    completionkey->continue_read(success, overlapped);
}
void SL::NET::IO_Context::handlewrite(bool success, Socket *completionkey, Win_IO_RW_Context *overlapped, DWORD trasnferedbytes)
{
    if (trasnferedbytes == 0) {
        std::cout << "Dropped Connection " << (int)success << std::endl;
        success = false;
    }
    overlapped->transfered_bytes += trasnferedbytes;
    completionkey->continue_write(success, overlapped);
}
void SL::NET::IO_Context::run(ThreadCount threadcount)
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
                    return;
                }
                switch (overlapped->IOOperation) {
                case IO_OPERATION::IoConnect:
                    handleconnect(bSuccess, completionkey, static_cast<Win_IO_Connect_Context *>(overlapped));
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
                auto pendio = --PendingIO;
                if (pendio == 0) {
                    return;
                }
            }
        }));
    }
}
