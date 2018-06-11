#include "Socket_Lite.h"
#include "defs.h"


namespace SL {
namespace NET {
    Context::Context(ThreadCount threadcount) { ContextImpl_ = new ContextImpl(threadcount); }

    Context::~Context() {  delete ContextImpl_; } 
} // namespace NET
} // namespace SL
