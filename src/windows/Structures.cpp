#include "Socket.h"
#include "Structures.h"
namespace SL {
namespace NET {

    PER_IO_CONTEXT *createcontext(NetworkProtocol protocol)
    {
        auto ret = new PER_IO_CONTEXT();
        ret->Socket = std::make_shared<Socket>(protocol);
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
} // namespace NET
} // namespace SL
