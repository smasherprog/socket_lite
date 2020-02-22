#ifndef SL_NETWORK_SOCKET_SEND_OP
#define SL_NETWORK_SOCKET_SEND_OP
#include "utils.h"

namespace SL::Network {
	class socket;

	template<class SOCKETTYPE> class socket_send_operation : public overlapped_operation {
	public:

		socket_send_operation(SOCKETTYPE& socket, std::byte* b, std::size_t byteCount) noexcept : socket(socket) {
			buffer.len = static_cast<decltype(buffer.len)>(byteCount);
			buffer.buf = reinterpret_cast<decltype(buffer.buf)>(b);
			socket.get_ioservice().incOp();
		}
		socket_send_operation(socket_send_operation&& other) noexcept : socket(other.socket), buffer(other.buffer) {}
		~socket_send_operation() {
			socket.get_ioservice().decOp();
		}

		auto await_ready() noexcept
		{
			DWORD transferedbytes = 0;
			if (::WSASend(socket.native_handle(), &buffer, 1, &transferedbytes, 0, getOverlappedStruct(), nullptr) == SOCKET_ERROR) {
				auto e = TranslateError();
				auto originalvalue = trysetstatus(e, StatusCode::SC_UNSET);
				if (originalvalue == StatusCode::SC_UNSET) {///successfully change from unset to the erro
					return e != StatusCode::SC_PENDINGIO1;
				}
			}

			return true;
		}

		auto await_resume() {
			return std::tuple(getstatus(), numberOfBytesTransferred);
		}

	private:
		SOCKETTYPE& socket;
		WSABUF buffer;
	};
}
#endif
