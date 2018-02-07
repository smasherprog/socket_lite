
#include "Context.h"
#include "Socket.h"
#include <assert.h>

namespace SL
{
namespace NET
{


Socket::Socket(Context* context, AddressFamily family) : Socket(context)
{
    if (family == AddressFamily::IPV4) {
        handle = socket(AF_INET, SOCK_STREAM, 0);
    } else {
        handle = socket(AF_INET6, SOCK_STREAM, 0);
    }
}
Socket::Socket(Context* context) : Context_(context) {}
Socket::~Socket()
{
}
void Socket::connect(SL::NET::sockaddr &address, const std::function<void(StatusCode)> &&)
{

}
void Socket::recv(size_t buffer_size, unsigned char *buffer, const std::function<void(StatusCode, size_t)> &&handler)
{

}
void Socket::send(size_t buffer_size, unsigned char *buffer, const std::function<void(StatusCode, size_t)> &&handler)
{

}

} // namespace NET
} // namespace SL
