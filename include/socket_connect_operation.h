#ifndef SL_NETWORK_SOCKET_CONNECT_OP
#define SL_NETWORK_SOCKET_CONNECT_OP

#include "utils.h"

namespace SL::Network {

	template<class SOCKETTYPE> class socket_connect_operation : public overlapped_operation {
	public:
		socket_connect_operation(SOCKETTYPE& socket, const SocketAddress& remoteEndPoint) noexcept : socket(socket), remoteEndPoint(remoteEndPoint), overlapped_operation({ 0 }) {
			socket.get_ioservice().incOp();
		}
		~socket_connect_operation() {
			socket.get_ioservice().decOp();
		}

		auto await_suspend(std::experimental::coroutine_handle<> coro) { awaitingCoroutine = coro; }
		auto await_ready() noexcept
		{
			static win32::ConnectExCreator Connector;
			DWORD bytesSent = 0;

			if (win32::win32Bind(remoteEndPoint.getFamily(), socket.native_handle()) == SOCKET_ERROR) {
				socket.close();
				errorCode = TranslateError();
				return true;
			}
			if (::CreateIoCompletionPort((HANDLE)socket.native_handle(), socket.get_ioservice().getHandle(), socket.native_handle(), 0) == NULL) {
				socket.close();
				errorCode = TranslateError();
				return true;
			}
			if (SetFileCompletionNotificationModes((HANDLE)socket.native_handle(), FILE_SKIP_COMPLETION_PORT_ON_SUCCESS) == FALSE) {
				socket.close();
				errorCode = TranslateError();
				return true;
			}

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

