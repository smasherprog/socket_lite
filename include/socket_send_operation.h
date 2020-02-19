#ifndef SL_NETWORK_SOCKET_SEND_OP
#define SL_NETWORK_SOCKET_SEND_OP
#include "utils.h"

namespace SL::Network {
	class socket;

	template<class SOCKETTYPE> class socket_send_operation : public overlapped_operation {
	public:

		socket_send_operation(SOCKETTYPE& socket, std::byte* b, std::size_t byteCount) noexcept : socket(socket), overlapped_operation({ 0 }) {
			buffer.len = static_cast<decltype(buffer.len)>(byteCount);
			buffer.buf = reinterpret_cast<decltype(buffer.buf)>(b);
			socket.get_ioservice().incOp();
		}
		~socket_send_operation() {
			socket.get_ioservice().decOp();
		}
		auto await_suspend(std::experimental::coroutine_handle<> coro) { awaitingCoroutine = coro; }
		auto await_ready() noexcept
		{
			DWORD numberOfBytesSent = 0; 
			if (::WSASend(socket.native_handle(), &buffer, 1, &numberOfBytesSent, 0, getOverlappedStruct(), nullptr) != 0) {
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
	};
}
#endif
