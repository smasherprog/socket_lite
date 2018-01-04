#include "Socket.h"
#include "Structures.h"
namespace SL {
namespace NET {

    PER_IO_CONTEXT *createcontext(NetworkProtocol protocol, Context *context)
    {
        auto ret = new PER_IO_CONTEXT();
        ret->Socket = std::make_shared<Socket>(protocol);
        ret->Socket->PER_IO_CONTEXT_ = ret;
        ret->Socket->Context_ = context;
        return ret;
    }

    void freecontext(PER_IO_CONTEXT **context)
    {
        if (context && *context) {
            (*context)->Socket.reset();
            delete *context;
            *context = nullptr;
        }
    }
    bool updateIOCP(SOCKET socket, HANDLE *iocphandle)
    {
        if (iocphandle && *iocphandle != NULL) {
            if (auto ctx = CreateIoCompletionPort((HANDLE)socket, *iocphandle, NULL, 0); ctx == NULL) {
                return false;
            }
            else {
                *iocphandle = ctx;
                return true;
            }
        }
        return false;
    }
    void async_read(PER_IO_CONTEXT *lpOverlapped, std::function<void(const std::shared_ptr<ISocket> &)> &onDisconnection)
    {
        DWORD flag(0), recvbyte(0);
        if (auto ret = WSARecv(lpOverlapped->Socket->handle, &lpOverlapped->wsabuf, 1, &recvbyte, &flag, &lpOverlapped->Overlapped, NULL);
            ret == SOCKET_ERROR && WSA_IO_PENDING != WSAGetLastError()) {
            // not good close down socket
            onDisconnection(lpOverlapped->Socket);
            freecontext(&lpOverlapped);
        }
    }
} // namespace NET
} // namespace SL
