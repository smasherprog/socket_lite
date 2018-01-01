#pragma once
#include "Structures.h"
namespace SL {
namespace NET {
    struct Context {
        WSARAII wsa;
        WSAEvent wsaevent;
        IOCP iocp;
    };
} // namespace NET
} // namespace SL