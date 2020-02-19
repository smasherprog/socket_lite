#include "socket.h"
#include "io_service.h"

#if _WIN32

namespace SL::Network {
	std::tuple<StatusCode, socket> socket::create(io_service& ioSvc, SocketType sockettype, AddressFamily family) {
		auto protocol = sockettype == SocketType::TCP ? IPPROTO_TCP : IPPROTO_UDP;

		auto socketHandle = ::socket(family, sockettype, protocol);
		if (socketHandle == INVALID_SOCKET) {
			return std::tuple(TranslateError(), socket(ioSvc));
		}

		if (sockettype == SOCK_STREAM) {
			// Turn off linger so that the destructor doesn't block while closing
			// the socket or silently continue to flush remaining data in the
			// background after ::closesocket() is called, which could fail and
			// we'd never know about it.
			// We expect clients to call Disconnect() or use CloseSend() to cleanly
			// shut-down connections instead.
			BOOL value = TRUE;
			if (::setsockopt(socketHandle, SOL_SOCKET, SO_DONTLINGER, reinterpret_cast<const char*>(&value), sizeof(value)) == SOCKET_ERROR) {
				closesocket(socketHandle);
				return std::tuple(TranslateError(), socket(ioSvc));
			}
		}

		return std::tuple(StatusCode::SC_SUCCESS, socket(socketHandle, ioSvc));
	}

} // namespace SL::Network

#endif