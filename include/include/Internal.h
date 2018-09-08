#pragma once
#include "defs.h"

namespace SL::NET::INTERNAL {

#if _WIN32
class AcceptExCreator {
  public:
    LPFN_ACCEPTEX AcceptEx_ = nullptr;
    AcceptExCreator() noexcept
    {
        auto temphandle = WSASocketW(AF_INET, SOCK_STREAM, IPPROTO_IP, NULL, 0, WSA_FLAG_OVERLAPPED);
        GUID acceptex_guid = WSAID_ACCEPTEX;
        DWORD bytes = 0;
        WSAIoctl(temphandle, SIO_GET_EXTENSION_FUNCTION_POINTER, &acceptex_guid, sizeof(acceptex_guid), &AcceptEx_, sizeof(AcceptEx_), &bytes, NULL,
                 NULL);
        assert(AcceptEx_ != nullptr);
        closesocket(temphandle);
    }
};
class ConnectExCreator {
  public:
    LPFN_CONNECTEX ConnectEx_ = nullptr;
    ConnectExCreator() noexcept
    {
        auto temphandle = WSASocketW(AF_INET, SOCK_STREAM, IPPROTO_IP, NULL, 0, WSA_FLAG_OVERLAPPED);
        GUID guid = WSAID_CONNECTEX;
        DWORD bytes = 0;
        ConnectEx_ = nullptr;
        WSAIoctl(temphandle, SIO_GET_EXTENSION_FUNCTION_POINTER, &guid, sizeof(guid), &ConnectEx_, sizeof(ConnectEx_), &bytes, NULL, NULL);
        assert(ConnectEx_ != nullptr);
        closesocket(temphandle);
    }
};
#endif
inline bool wouldBlock() noexcept
{
#if _WIN32
    return WSAGetLastError() == WSAEWOULDBLOCK;
#else
    return errno != EAGAIN && errno != EINTR;
#endif
}
int inline Send(SocketHandle handle, unsigned char *buf, int len) noexcept
{
#if _WIN32
    return ::send(handle.value, reinterpret_cast<char *>(buf), static_cast<int>(len), 0);
#else
    return ::send(handle.value, buf, static_cast<int>(len), MSG_NOSIGNAL);
#endif
}

int inline Recv(SocketHandle handle, unsigned char *buf, int len) noexcept
{
#if _WIN32
    return ::recv(handle.value, reinterpret_cast<char *>(buf), static_cast<int>(len), 0);
#else
    return ::recv(handle.value, buf, static_cast<int>(len), MSG_NOSIGNAL);
#endif
}

template <class SOCKETHANDLERTYPE>
void setup(RW_Context &rwcontext, std::atomic<int> &iocount, IO_OPERATION op, int buffer_size, unsigned char *buffer,
           const SOCKETHANDLERTYPE &handler) noexcept
{
    assert(!rwcontext.hasCompletionHandler()); // cannot call if there is a pending operation
    rwcontext.buffer = buffer;
    rwcontext.setRemainingBytes(buffer_size);
    rwcontext.setCompletionHandler(handler);
    rwcontext.setEvent(op);
    iocount.fetch_add(1, std::memory_order_acquire);
} 
inline void completeio(RW_Context &rwcontext, std::atomic<int> &iocount, StatusCode code) noexcept
{
    if (auto h(rwcontext.getCompletionHandler()); h) {
        iocount.fetch_sub(1, std::memory_order_acquire);
        h(code);
    }
}

} // namespace SL::NET::INTERNAL
