
#include "IO_Context.h"
#include "Socket.h"
#include "common/List.h"
#include <assert.h>

namespace SL {
namespace NET {

    std::shared_ptr<ISocket> SOCKET_LITE_EXTERN CreateSocket(const std::shared_ptr<IIO_Context> &iocontext)
    {
        auto context = static_cast<IO_Context *>(iocontext.get());
        return std::make_shared<Socket>(context->PendingIO);
    }

    Socket::Socket(std::atomic<size_t> &counter) : PendingIO(counter) { handle = INVALID_SOCKET; }
    Socket::~Socket()
    { // make sure the links are all cleaned up
        close();
    }

    bool Socket::UpdateIOCP(SOCKET socket, HANDLE *iocp, void *completionkey)
    {
        if (*iocp = CreateIoCompletionPort((HANDLE)socket, *iocp, (DWORD_PTR)completionkey, 0); *iocp != NULL) {
            return true;
        }
        else {
            std::cerr << "CreateIoCompletionPort() failed: " << GetLastError() << std::endl;
            return false;
        }
    }

    void Socket::close()
    {
        if (handle != INVALID_SOCKET) {
            closesocket(handle);
        }

        handle = INVALID_SOCKET;
    }
    template <typename T> void IOComplete(Socket *s, Bytes_Transfered bytes, T *context)
    {
        if (bytes == -1) {
            s->close();
        }
        s->PendingIO -= 1;
        context->completionhandler(bytes);
    }

    void Socket::async_read(size_t buffer_size, unsigned char *buffer, const std::function<void(Bytes_Transfered)> &&handler)
    {
        if (handle == INVALID_SOCKET) {
            handler(-1);
        }
        PendingIO += 1;
        assert(ReadContext.IOOperation == IO_OPERATION::IoNone);
        ReadContext.clear();
        ReadContext.buffer = buffer;
        ReadContext.bufferlen = buffer_size;
        ReadContext.completionhandler = std::move(handler);
        ReadContext.IOOperation = IO_OPERATION::IoRead;
        ReadContext.wsabuf.buf = nullptr;
        ReadContext.wsabuf.len = 0;
        DWORD dwSendNumBytes(0), dwFlags(0);
        // do read with zero bytes to prevent memory from being mapped
        if (auto nRet = WSARecv(handle, &ReadContext.wsabuf, 1, &dwSendNumBytes, &dwFlags, &(ReadContext.Overlapped), NULL);
            nRet == SOCKET_ERROR && (ERROR_IO_PENDING != WSAGetLastError())) {
            return IOComplete(this, -1, &ReadContext);
        }
    }

    void Socket::continue_read(Win_IO_RW_Context *sockcontext)
    {
        // read any data available
        if (sockcontext->bufferlen < sockcontext->transfered_bytes) {
            auto bytes_read = 0;
            do {
                auto remainingbytes = sockcontext->bufferlen - sockcontext->transfered_bytes;
                bytes_read = recv(handle, (char *)sockcontext->buffer + sockcontext->transfered_bytes, remainingbytes, 0);
                if (bytes_read > 0) {
                    sockcontext->transfered_bytes += bytes_read;
                }
                else if (bytes_read == 0 || (bytes_read == SOCKET_ERROR && WSAGetLastError() != WSAEWOULDBLOCK)) {
                    return IOComplete(this, -1, sockcontext);
                }
            } while (bytes_read > 0);
        }

        if (sockcontext->bufferlen == sockcontext->transfered_bytes) {
            return IOComplete(this, sockcontext->transfered_bytes, sockcontext);
        }
        else {
            PendingIO += 1;
            // still more data to read.. keep going!
            sockcontext->wsabuf.buf = nullptr;
            sockcontext->wsabuf.len = 0;
            DWORD dwSendNumBytes(0), dwFlags(0);
            // do read with zero bytes to prevent memory from being mapped
            if (auto nRet = WSARecv(handle, &sockcontext->wsabuf, 1, &dwSendNumBytes, &dwFlags, &(sockcontext->Overlapped), NULL);
                nRet == SOCKET_ERROR && (ERROR_IO_PENDING != WSAGetLastError())) {
                return IOComplete(this, -1, sockcontext);
            }
        }
    }
    template <typename T> void TryConnectAgain(Socket *s, T *context)
    {
        s->PendingIO -= 1;
        s->continue_connect(ConnectionAttemptStatus::FailedConnect, context);
    }
    void Socket::async_connect(const std::shared_ptr<IIO_Context> &io_context, std::vector<sockaddr> &addresses,
                               const std::function<ConnectSelection(ConnectionAttemptStatus, sockaddr &)> &&handler)
    {
        if (addresses.empty())
            return;

        auto iocontext = static_cast<IO_Context *>(io_context.get());
        auto context = new Win_IO_Connect_Context();
        context->completionhandler = std::move(handler);
        context->IOOperation = IO_OPERATION::IoConnect;
        context->iocp = iocontext->iocp.handle;
        context->RemainingAddresses = std::move(addresses);

        continue_connect(context);
    }
    void Socket::continue_connect(ConnectionAttemptStatus connect_success, Win_IO_Connect_Context *sockcontext)
    {
        PendingIO -= 1;
        if (::setsockopt(handle, SOL_SOCKET, SO_UPDATE_CONNECT_CONTEXT, 0, 0) == SOCKET_ERROR) {
            std::cerr << "Error setsockopt SO_UPDATE_CONNECT_CONTEXT Code: " << WSAGetLastError() << std::endl;
            connect_success = ConnectionAttemptStatus::FailedConnect;
        }
        if (connect_success == ConnectionAttemptStatus::SuccessfullConnect) {
            if (sockcontext->RemainingAddresses.empty() ||
                sockcontext->completionhandler(connect_success, sockcontext->RemainingAddresses.back()) == ConnectSelection::Selected) {
                delete sockcontext;
                return;
            }
        }
        sockcontext->RemainingAddresses.pop_back();
        if (sockcontext->RemainingAddresses.empty()) {
            delete sockcontext;
            return;
        }
        continue_connect(sockcontext);
    }
    void Socket::continue_connect(Win_IO_Connect_Context *sockcontext)
    {
        close();
        PendingIO += 1;
        auto addr = sockcontext->RemainingAddresses.back();
        if (!bind(addr)) {
            return TryConnectAgain(this, sockcontext);
        }
        GUID guid = WSAID_CONNECTEX;
        DWORD bytes = 0;
        if (WSAIoctl(handle, SIO_GET_EXTENSION_FUNCTION_POINTER, &guid, sizeof(guid), &sockcontext->ConnectEx_, sizeof(sockcontext->ConnectEx_),
                     &bytes, NULL, NULL) != SOCKET_ERROR) {
            if (!Socket::UpdateIOCP(handle, &sockcontext->iocp, this)) {
                return TryConnectAgain(this, sockcontext);
            }
        }
        else {
            std::cerr << "failed to load ConnectEX: " << WSAGetLastError() << std::endl;
            return TryConnectAgain(this, sockcontext);
        }

        if (addr.Family == Address_Family::IPV4) {
            ::sockaddr_in sockaddr = {0};
            int sockaddrlen = sizeof(sockaddr);
            memcpy(addr.Address, &sockaddr.sin_addr, sizeof(sockaddr.sin_addr));
            sockaddr.sin_port = addr.Port;
            sockaddr.sin_family = AF_INET;
            DWORD bytessend = 0;
            auto connectres =
                sockcontext->ConnectEx_(handle, (::sockaddr *)&sockaddr, sockaddrlen, NULL, 0, &bytessend, (LPOVERLAPPED)&sockcontext->Overlapped);
            if (connectres == TRUE) {
                return continue_connect(ConnectionAttemptStatus::SuccessfullConnect, sockcontext);
            }
            else if (connectres == FALSE && WSAGetLastError() == ERROR_IO_PENDING) {
                return;
            }
        }
        else {
            sockaddr_in6 sockaddr = {0};
            int sockaddrlen = sizeof(sockaddr);
            memcpy(addr.Address, &sockaddr.sin6_addr, sizeof(sockaddr.sin6_addr));
            sockaddr.sin6_port = addr.Port;
            sockaddr.sin6_family = AF_INET6;
            DWORD bytessend = 0;
            auto connectres =
                sockcontext->ConnectEx_(handle, (::sockaddr *)&sockaddr, sockaddrlen, NULL, 0, &bytessend, (LPOVERLAPPED)&sockcontext->Overlapped);
            if (connectres == TRUE) {
                return continue_connect(ConnectionAttemptStatus::SuccessfullConnect, sockcontext);
            }
            else if (connectres == FALSE && WSAGetLastError() == ERROR_IO_PENDING) {
                return;
            }
        }
        TryConnectAgain(this, sockcontext);
    }
    void Socket::async_write(size_t buffer_size, unsigned char *buffer, const std::function<void(Bytes_Transfered)> &&handler)
    {
        auto val = new Win_IO_RW_Context();
        val->buffer = buffer;
        val->bufferlen = buffer_size;
        val->completionhandler = handler;
        val->IOOperation = IO_OPERATION::IoWrite;
        continue_write(val);
    }
    void Socket::continue_write(Win_IO_RW_Context *sockcontext)
    {
        PendingIO += 1;
        if (sockcontext->transfered_bytes == -1) {
            return IOComplete(this, -1, sockcontext);
        }
        else if (sockcontext->transfered_bytes == sockcontext->bufferlen) {
            return IOComplete(this, sockcontext->transfered_bytes, sockcontext);
        }
        else {
            sockcontext->wsabuf.buf = (char *)sockcontext->buffer + sockcontext->transfered_bytes;
            sockcontext->wsabuf.len = sockcontext->bufferlen - sockcontext->transfered_bytes;
            DWORD dwSendNumBytes(0), dwFlags(0);
            if (auto nRet = WSASend(handle, &sockcontext->wsabuf, 1, &dwSendNumBytes, dwFlags, &(sockcontext->Overlapped), NULL);
                nRet == SOCKET_ERROR && (ERROR_IO_PENDING != WSAGetLastError())) {
                return IOComplete(this, -1, sockcontext);
            }
        }
    }

} // namespace NET
} // namespace SL
