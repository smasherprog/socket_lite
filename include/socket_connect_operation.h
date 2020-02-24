#ifndef SL_NETWORK_SOCKET_CONNECT_OP
#define SL_NETWORK_SOCKET_CONNECT_OP

#include "utils.h"
#include "io_service.h"

namespace SL::Network {

	class socket_connect_operation : public overlapped_operation {
	public:
		socket_connect_operation(SOCKET socket, refcounter& refcounter, const SocketAddress& remoteEndPoint) noexcept : socket(socket), remoteEndPoint(remoteEndPoint), refcounter(refcounter) {
			refcounter.incOp();
		}
		socket_connect_operation(socket_connect_operation&& other) noexcept : socket(other.socket), remoteEndPoint(other.remoteEndPoint), refcounter(other.refcounter) {}
		~socket_connect_operation() {
			refcounter.decOp();
		}

		auto await_ready() noexcept
		{
			if (win32::win32Bind(remoteEndPoint.getFamily(), socket) == SOCKET_ERROR) {
				setstatus(TranslateError());
				return true;
			}

			if (SetFileCompletionNotificationModes((HANDLE)socket, FILE_SKIP_COMPLETION_PORT_ON_SUCCESS) == FALSE) {
				setstatus(TranslateError());
				return true;
			}

			DWORD transferedbytes = 0;
			if (win32::ConnectEx_(socket, remoteEndPoint.getSocketAddr(), remoteEndPoint.getSocketAddrLen(), 0, 0, &transferedbytes, getOverlappedStruct()) == FALSE) {
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

			if (::setsockopt(socket, SOL_SOCKET, SO_UPDATE_CONNECT_CONTEXT, nullptr, 0) == SOCKET_ERROR) {
				status = TranslateError();
			}
			return status;
		}

	private:
		SOCKET socket;
		refcounter& refcounter;
		const SocketAddress& remoteEndPoint;
	};

	template<class T> inline void socket_connect(socket& socket, const SocketAddress& remoteEndPoint, T& cb) {







	}
}
#endif

