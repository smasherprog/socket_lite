#include "Socket.h"
#include "Structures.h"
namespace SL {
namespace NET {

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
