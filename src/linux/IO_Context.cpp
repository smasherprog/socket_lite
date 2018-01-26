#include "IO_Context.h"
#include "Listener.h"
#include "Socket.h"
#include <chrono>

using namespace std::chrono_literals;

std::shared_ptr<SL::NET::IIO_Context> SL::NET::CreateIO_Context()
{
    return std::make_shared<IO_Context>();
}
SL::NET::IO_Context::IO_Context()
{
    PendingIO = 0;
}

SL::NET::IO_Context::~IO_Context()
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
            } else {
                t.join();
            }
        }
    }
}

void SL::NET::IO_Context::handleconnect(bool success, Socket *completionkey, Win_IO_RW_Context *overlapped)
{
    completionkey->continue_connect(success ? ConnectionAttemptStatus::SuccessfullConnect : ConnectionAttemptStatus::FailedConnect, overlapped);
}
void SL::NET::IO_Context::handlerecv(bool success, Socket *completionkey, Win_IO_RW_Context *overlapped, DWORD trasnferedbytes)
{
    if (trasnferedbytes == 0) {
        success = false;
    }
    overlapped->transfered_bytes += trasnferedbytes;
    completionkey->continue_read(success, overlapped);
}
void SL::NET::IO_Context::handlewrite(bool success, Socket *completionkey, Win_IO_RW_Context *overlapped, DWORD trasnferedbytes)
{
    if (trasnferedbytes == 0) {
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
            std::vecdtor<epoll_event> epollevents;
            epollevents.resize(MAXEVENTS);
            while (true) {
                int efd =-1;
                auto count = epoll_wait(iocp.handle, epollevents.data(),MAXEVENTS, -1 );
                for(auto i=0; i< count ; i++) {
                    if(epolllevents[i].data.fd == Listener_->ListenSocket->get_handle()) {
                        handleaccept(epolllevents[i]);
                    }
                    switch (overlapped->IOOperation) {
                    case IO_OPERATION::IoConnect:
                        handleconnect(bSuccess, completionkey, static_cast<Win_IO_RW_Context *>(overlapped));
                        break;
                    case IO_OPERATION::IoAccept:

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
            }
        }
    }));
}
}
