#ifndef SL_NETWORK_SOCKET_SEND_TO_OP
#define SL_NETWORK_SOCKET_SEND_TO_OP
#include "utils.h"

namespace SL::Network {
	class socket;

	template<class SOCKETTYPE> class socket_send_to_operation : public overlapped_operation {
	public:

		socket_send_to_operation(SOCKETTYPE& socket, const SocketAddress& remoteEndPoint, std::byte* b, std::size_t byteCount) noexcept : socket(socket), remoteEndPoint(remoteEndPoint) {
			buffer.len = static_cast<decltype(buffer.len)>(byteCount);
			buffer.buf = reinterpret_cast<decltype(buffer.buf)>(b);
		}

		auto await_suspend(std::experimental::coroutine_handle<> coro) { awaitingCoroutine = coro; }
		auto await_ready() noexcept
		{
			DWORD numberOfBytesReceived = 0;
			DWORD flags = 0;
			auto socklen = remoteEndPoint.getSocketAddrLen();
			if (::WSASendTo(m_socket.native_handle(), &buffer, 1, &numberOfBytesSent, &flags, remoteEndPoint.getSocketAddr(), socklen, getOverlappedStruct(), nullptr) == 0) {
				return StatusCode::SC_SUCCESS;
			}
			else {
				errorCode = TranslateError();
				return errorCode != StatusCode::SC_PENDINGIO;
			}

			return true;
		}

		auto await_resume() {
			return std::tuple(errorCode, numberOfBytesTransferred);
		}

	private:
		SOCKETTYPE& socket;
		WSABUF buffer;
		const SocketAddress& remoteEndPoint;
	};
}
#endif
