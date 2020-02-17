#ifndef SL_NETWORK_SOCKET_RECV_FROM_OP
#define SL_NETWORK_SOCKET_RECV_FROM_OP
#include "utils.h"

namespace SL::Network {
	class socket;

	template<class SOCKETTYPE> class socket_recv_from_operation : public overlapped_operation {
	public:

		socket_recv_from_operation(SOCKETTYPE& socket, std::byte* b, std::size_t byteCount) noexcept : socket(socket) {
			buffer.len = static_cast<decltype(buffer.len)>(byteCount);
			buffer.buf = reinterpret_cast<decltype(buffer.buf)>(b);
		}

		auto await_suspend(std::experimental::coroutine_handle<> coro) { awaitingCoroutine = coro; }
		auto await_ready() noexcept
		{
			DWORD numberOfBytesReceived = 0;
			DWORD flags = 0;

			if (::WSARecvFrom(m_socket.native_handle(), &buffer, 1, &numberOfBytesSent, &flags, reinterpret_cast<sockaddr*>(&storage), &socklen, getOverlappedStruct(), nullptr) == 0) {
				return StatusCode::SC_SUCCESS;
			}
			else {
				errorCode = TranslateError();
				return errorCode != StatusCode::SC_PENDINGIO;
			}

			return true;
		}

		auto await_resume() {
			return std::tuple(errorCode, SocketAddress(storage, socklen) numberOfBytesTransferred);
		}

	private:
		SOCKETTYPE& socket;
		WSABUF buffer;
		DWORD socklen = 0;
		::sockaddr_storage storage;
	};
}
#endif
