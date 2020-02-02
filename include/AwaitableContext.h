#pragma once
#include "Internal.h"
#include "PlatformSocket.h" 

namespace SL::NET {

    class AwaitableContext {

        void continue_read_async(bool success, INTERNAL::Awaiter* awaiter, DWORD bytestransfered)
        {
            auto rw = static_cast<INTERNAL::ReadAwaiter*>(awaiter);
            if (!success) {
                ReadContexts[rw->SocketHandle_.value] = nullptr;
                return rw->IOError();
            }
            rw->buffer += static_cast<int>(bytestransfered);
            rw->remainingbytes -= static_cast<int>(bytestransfered);
            if (rw->remainingbytes == 0) {
                ReadContexts[rw->SocketHandle_.value] = nullptr;
                return rw->IOSuccess();
            }
            else {
                WSABUF wsabuf;
                wsabuf.buf = reinterpret_cast<char*>(rw->buffer);
                wsabuf.len = static_cast<decltype(wsabuf.len)>(rw->remainingbytes);
                DWORD dwSendNumBytes(0), dwFlags(0);
                DWORD nRet = WSARecv(rw->SocketHandle_.value, &wsabuf, 1, &dwSendNumBytes, &dwFlags, &(rw->Overlapped), NULL);
                if (auto lasterr = WSAGetLastError(); nRet == SOCKET_ERROR && (WSA_IO_PENDING != lasterr)) {
                    ReadContexts[rw->SocketHandle_.value] = nullptr;
                    return rw->IOError();
                }
            }
        }
        void continue_write_async(bool success, INTERNAL::Awaiter* awaiter, DWORD bytestransfered)
        {
            auto rw = static_cast<INTERNAL::WriteAwaiter*>(awaiter);
            if (!success) {
                WriteContexts[rw->SocketHandle_.value] = nullptr;
                return rw->IOError();
            }
            rw->buffer += static_cast<int>(bytestransfered);
            rw->remainingbytes -= static_cast<int>(bytestransfered);
            if (rw->remainingbytes == 0) {
                WriteContexts[rw->SocketHandle_.value] = nullptr;
                return rw->IOSuccess();
            }
            else {
                WSABUF wsabuf;
                wsabuf.buf = reinterpret_cast<char*>(rw->buffer);
                wsabuf.len = static_cast<decltype(wsabuf.len)>(rw->remainingbytes);
                DWORD dwSendNumBytes(0), dwFlags(0);
                DWORD nRet = WSASend(rw->SocketHandle_.value, &wsabuf, 1, &dwSendNumBytes, dwFlags, &(rw->Overlapped), NULL);
                if (auto lasterr = WSAGetLastError(); nRet == SOCKET_ERROR && (WSA_IO_PENDING != lasterr)) {
                    WriteContexts[rw->SocketHandle_.value] = nullptr;
                    return rw->IOError();
                }
            }
        }
        WSADATA wsaData;
        std::vector<INTERNAL::Awaiter*> ReadContexts, WriteContexts;
        bool Keepgoing;
    public:

        HANDLE IOCPHandle;


        AwaitableContext()
        {
            Keepgoing = true;
			auto threadcount = 4;
            ReadContexts.resize(std::numeric_limits<unsigned short>::max());
            WriteContexts.resize(std::numeric_limits<unsigned short>::max());

            ReadContexts.shrink_to_fit();
            WriteContexts.shrink_to_fit();

            for (auto& c : ReadContexts) {
                c = nullptr;
            }
            for (auto& c : WriteContexts) {
                c = nullptr;
            }
            IOCPHandle = nullptr;
            if (WSAStartup(0x202, &wsaData) != 0) {
                abort();
            }
            IOCPHandle = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, threadcount);
            assert(IOCPHandle != NULL);

            for (decltype(threadcount) i = 0; i < threadcount; i++) {
                std::thread([&] {
                    while (Keepgoing) {
                        DWORD numberofbytestransfered = 0;
                        INTERNAL::Awaiter *overlapped = nullptr;
                        void *completionkey = nullptr;

                        auto success = GetQueuedCompletionStatus(IOCPHandle, &numberofbytestransfered, (PDWORD_PTR)&completionkey, (LPOVERLAPPED *)&overlapped, INFINITE) == TRUE;

                        if (overlapped != nullptr) {
                            switch (auto eventyype = overlapped->Operation) {
                            case IO_OPERATION::IoRead:
                                continue_read_async(success, overlapped, numberofbytestransfered); 
                                break;
                            case IO_OPERATION::IoWrite:
                                continue_write_async(success, overlapped, numberofbytestransfered);  
                                break;
                            case IO_OPERATION::IoConnect:
                            case IO_OPERATION::IoAccept:
                                ReadContexts[overlapped->SocketHandle_.value] = nullptr;
                                overlapped->ResumeHandle.resume();
                                break;
                            default:
                                break;
                            }
                        }
                    }
                    PostQueuedCompletionStatus(IOCPHandle, 0, (DWORD)NULL, NULL);
                }).detach();
            }
        }
        ~AwaitableContext()
        {
            Keepgoing = false;
            PostQueuedCompletionStatus(IOCPHandle, 0, (DWORD)NULL, NULL);

            if (IOCPHandle != nullptr) {
                CloseHandle(IOCPHandle);
                WSACleanup();
                IOCPHandle = nullptr;
            }
        }

        StatusCode RegisterSocket(const SocketHandle &socket)
        {
            if (CreateIoCompletionPort((HANDLE)socket.value, IOCPHandle, socket.value, NULL) == NULL) {
                return TranslateError();
            }
            return StatusCode::SC_SUCCESS;
        }
        void SetReadIO(const SocketHandle &socket, INTERNAL::Awaiter* awaiter)
        {
            if (socket.value >= 0 && socket.value < ReadContexts.size()) {
                ReadContexts[socket.value] = awaiter;
            }
        }
        void SetWriteIO(const SocketHandle &socket, INTERNAL::Awaiter* awaiter)
        {
            if (socket.value >= 0 && socket.value < WriteContexts.size()) {
                WriteContexts[socket.value] = awaiter;
            }
        }
        void DeregisterSocket(const SocketHandle &socket)
        {
            if (socket.value >= 0 && socket.value < ReadContexts.size()) {
                if (auto h = WriteContexts[socket.value]; h) {
                    h->StatusCode_ = StatusCode::SC_CLOSED;
                    h->ResumeHandle.resume();
                    WriteContexts[socket.value] = nullptr;
                }
                if (auto h = ReadContexts[socket.value]; h) {
                    h->StatusCode_ = StatusCode::SC_CLOSED;
                    h->ResumeHandle.resume();
                    ReadContexts[socket.value] = nullptr;
                }
            } 
        }
    };
}
