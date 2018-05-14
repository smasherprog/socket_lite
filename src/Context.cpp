#include "Socket_Lite.h"
#include "defs.h"

#include <algorithm>
#include <chrono>
#if _WIN32
#include <Ws2tcpip.h>
#else
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <unistd.h>
#endif

using namespace std::chrono_literals;
namespace SL {
namespace NET {
    Context::Context(ThreadCount threadcount) { ContextImpl_ = new ContextImpl(threadcount); }

    Context::~Context() {  delete ContextImpl_; }
    IOData &Context::getIOData() { return ContextImpl_->getIOData(); }
} // namespace NET
} // namespace SL
