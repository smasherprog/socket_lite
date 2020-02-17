#ifndef SL_NETWORK_SOCKET_CONNECT_OP
#define SL_NETWORK_SOCKET_CONNECT_OP

#include "utils.h"

namespace SL::Network {

	template<class SOCKETTYPE> class socket_connect_operation : public overlapped_operation {
	public:
		socket_connect_operation(SOCKETTYPE& socket, const SocketAddress& remoteEndPoint) noexcept : socket(socket), remoteEndPoint(remoteEndPoint) {}
		auto await_suspend(std::experimental::coroutine_handle<> coro) { awaitingCoroutine = coro; }
		auto await_ready() noexcept
		{
			static win32::ConnectExCreator Connector;
			DWORD bytesSent = 0;
			if (Connector.ConnectEx_(socket.native_handle(), remoteEndPoint.getSocketAddr(), remoteEndPoint.getSocketAddrLen(), nullptr, 0, &bytesSent, getOverlappedStruct()) == FALSE) {
				errorCode = TranslateError();
				return errorCode != StatusCode::SC_PENDINGIO;
			}

			return true;
		}
		auto await_resume() {
			if (errorCode != StatusCode::SC_SUCCESS) {
				return errorCode;
			}

			if (::setsockopt(socket.native_handle(), SOL_SOCKET, SO_UPDATE_CONNECT_CONTEXT, nullptr, 0) == SOCKET_ERROR) {
				errorCode = TranslateError();
			}
			return errorCode;
		}

	private:
		SOCKETTYPE& socket;
		const SocketAddress& remoteEndPoint;
	};
}
#endif

