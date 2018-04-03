
#include "Socket_Lite.h"
#include "defs.h"
#include <Ws2ipdef.h>
#include <assert.h>
namespace SL {
namespace NET {

    void recv(Socket &socket, size_t buffer_size, unsigned char *buffer, std::function<void(StatusCode, size_t)> &&handler)
    {
        if (!socket.isopen())
            return;
        SocketGetter sg(socket);
        auto context = new Win_IO_RW_Context();
        context->buffer = buffer;
        context->transfered_bytes = 0;
        context->bufferlen = buffer_size;
        context->Context_ = sg.getContext();
        context->Socket_ = sg.getSocket();
        context->setCompletionHandler(std::move(handler));
        context->IOOperation = IO_OPERATION::IoRead;
        continue_io(true, context, sg.getPendingIO());
    }
    void send(Socket &socket, size_t buffer_size, unsigned char *buffer, std::function<void(StatusCode, size_t)> &&handler)
    {
        if (!socket.isopen())
            return;
        SocketGetter sg(socket);
        auto context = new Win_IO_RW_Context();
        context->buffer = buffer;
        context->transfered_bytes = 0;
        context->bufferlen = buffer_size;
        context->Context_ = sg.getContext();
        context->Socket_ = sg.getSocket();
        context->setCompletionHandler(std::move(handler));
        context->IOOperation = IO_OPERATION::IoWrite;
        continue_io(true, context, sg.getPendingIO());
    }
    void continue_io(bool success, Win_IO_RW_Context *context, std::atomic<int> &pendingio)
    {
        if (!success) {
            if (auto handler(context->getCompletionHandler()); handler) {
                delete context;
                handler(TranslateError(), 0);
            }
        }
        else if (context->bufferlen == context->transfered_bytes) {
            if (auto handler(context->getCompletionHandler()); handler) {
                delete context;
                handler(StatusCode::SC_SUCCESS, context->transfered_bytes);
            }
        }
        else {
            WSABUF wsabuf;
            auto bytesleft = static_cast<decltype(wsabuf.len)>(context->bufferlen - context->transfered_bytes);
            wsabuf.buf = (char *)context->buffer + context->transfered_bytes;
            wsabuf.len = bytesleft;
            DWORD dwSendNumBytes(0), dwFlags(0);
            pendingio += 1;
            DWORD nRet = 0;
            if (context->IOOperation == IO_OPERATION::IoRead) {
                nRet = WSARecv(context->Socket_, &wsabuf, 1, &dwSendNumBytes, &dwFlags, &(context->Overlapped), NULL);
            }
            else {
                nRet = WSASend(context->Socket_, &wsabuf, 1, &dwSendNumBytes, dwFlags, &(context->Overlapped), NULL);
            }
            auto lasterr = WSAGetLastError();
            if (nRet == SOCKET_ERROR && (WSA_IO_PENDING != lasterr)) {
                if (auto handler(context->getCompletionHandler()); handler) {
                    pendingio -= 1;
                    delete context;
                    handler(TranslateError(&lasterr), 0);
                }
            }
        }
    }
    void BindSocket(SOCKET sock, AddressFamily family)
    {
        if (family == AddressFamily::IPV4) {
            sockaddr_in bindaddr = {0};
            bindaddr.sin_family = AF_INET;
            bindaddr.sin_addr.s_addr = INADDR_ANY;
            bindaddr.sin_port = 0;
            ::bind(sock, (::sockaddr *)&bindaddr, sizeof(bindaddr));
        }
        else {
            sockaddr_in6 bindaddr = {0};
            bindaddr.sin6_family = AF_INET6;
            bindaddr.sin6_addr = in6addr_any;
            bindaddr.sin6_port = 0;
            ::bind(sock, (::sockaddr *)&bindaddr, sizeof(bindaddr));
        }
    }

    void continue_connect(bool success, Win_IO_Connect_Context *context)
    {
        auto h = context->getCompletionHandler();
        if (success && ::setsockopt(context->Socket_, SOL_SOCKET, SO_UPDATE_CONNECT_CONTEXT, 0, 0) != SOCKET_ERROR) {
            if (h) {
                delete context;
                h(StatusCode::SC_SUCCESS);
            }
            else {
                delete context;
            }
        }
        else if (h) {
            delete context;
            h(TranslateError());
        }
        else {
            delete context;
        }
    }
    void connect(Socket &socket, sockaddr &address, std::function<void(StatusCode)> &&handler)
    {
        SocketGetter sg(socket);
        auto handle = sg.setSocket(INTERNAL::Socket(address.get_Family()));
        BindSocket(handle, address.get_Family());
        if (CreateIoCompletionPort((HANDLE)handle, sg.getIOCPHandle(), NULL, NULL) == NULL) {
            return handler(TranslateError());
        }

        auto context = new Win_IO_Connect_Context();
        context->Context_ = sg.getContext();
        context->setCompletionHandler(std::move(handler));
        context->IOOperation = IO_OPERATION::IoConnect;
        context->Socket_ = handle;

        sg.getPendingIO() += 1;
        auto connectres =
            sg.ConnectEx()(handle, (::sockaddr *)address.get_SocketAddr(), address.get_SocketAddrLen(), 0, 0, 0, (LPOVERLAPPED)&context->Overlapped);
        if (connectres == TRUE) {
            sg.getPendingIO() -= 1;
            continue_connect(true, context);
        }
        else if (auto err = WSAGetLastError(); !(connectres == FALSE && err == ERROR_IO_PENDING)) {

            if (auto h(context->getCompletionHandler()); h) {
                sg.getPendingIO() -= 1;
                delete context;
                h(TranslateError(&err));
            }
        }
    }

} // namespace NET
} // namespace SL
