#ifndef SL_NETWORK_SOCKET_CONNECT_OP
#define SL_NETWORK_SOCKET_CONNECT_OP

#include "utils.h"

namespace SL::Network {

	template<class SOCKETTYPE> class socket_connect_operation : public overlapped_operation {
	public:
		socket_connect_operation(SOCKETTYPE& socket, const SocketAddress& remoteEndPoint) noexcept : socket(socket), remoteEndPoint(remoteEndPoint) {
			socket.get_ioservice().incOp();
		}
		socket_connect_operation(socket_connect_operation&& other) noexcept : socket(other.socket), remoteEndPoint(other.remoteEndPoint) {}
		~socket_connect_operation() {
			socket.get_ioservice().decOp();
		}

		auto await_ready() noexcept
		{ 
			if (win32::win32Bind(remoteEndPoint.getFamily(), socket.native_handle()) == SOCKET_ERROR) {
				socket.close();
				setstatus(TranslateError());
				return true;
			}

			if (::CreateIoCompletionPort((HANDLE)socket.native_handle(), socket.get_ioservice().getHandle(), socket.native_handle(), 0) == NULL) {
				socket.close();
				setstatus(TranslateError());
				return true;
			}
			if (SetFileCompletionNotificationModes((HANDLE)socket.native_handle(), FILE_SKIP_COMPLETION_PORT_ON_SUCCESS) == FALSE) {
				socket.close();
				setstatus(TranslateError());
				return true;
			}

			DWORD transferedbytes = 0;
			if (win32::ConnectEx_(socket.native_handle(), remoteEndPoint.getSocketAddr(), remoteEndPoint.getSocketAddrLen(), 0, 0, &transferedbytes, getOverlappedStruct()) == FALSE) {
				auto e = TranslateError();
				auto originalvalue = trysetstatus(e, StatusCode::SC_UNSET);
				if (originalvalue == StatusCode::SC_UNSET) {///successfully change from unset to the erro
					return e != StatusCode::SC_PENDINGIO1;
				}
			}

			return true;
		}
		auto await_resume() {
			auto status = getstatus();
			if (status != StatusCode::SC_SUCCESS) {
				return status;
			}

			if (::setsockopt(socket.native_handle(), SOL_SOCKET, SO_UPDATE_CONNECT_CONTEXT, nullptr, 0) == SOCKET_ERROR) {
				status = TranslateError();
			}
			return status;
		}

	private:
		SOCKETTYPE& socket;
		const SocketAddress& remoteEndPoint;
	};
}
#endif

