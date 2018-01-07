#include "internal/SocketFuncs.h"
#ifdef WIN32
#include "internal/windows/Structures.h"
#else
#include <sys/socket.h>
#include <sys/types.h>
#define INVALID_SOCKET -1
#define SOCKET_ERROR -1
#define WSAEWOULDBLOCK EAGAIN
#define closesocket close
#endif
#include <string>

namespace SL {
namespace NET {

} // namespace NET
} // namespace SL