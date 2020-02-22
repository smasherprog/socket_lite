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
			socket.get_ioservice().incOp();
		}
		~socket_send_to_operation() {
			socket.get_ioservice().decOp();
		}


		auto await_ready() noexcept
		{
			numberOfBytesTransferred = 0;
			DWORD flags = 0;
			if (::WSASendTo(m_socket.native_handle(), &buffer, 1, &numberOfBytesTransferred, &flags, remoteEndPoint.getSocketAddr(), remoteEndPoint.getSocketAddrLen(), getOverlappedStruct(), nullptr) == SOCKET_ERROR) {
			
				return getstatus() != StatusCode::SC_PENDINGIO1;
			}

			return true;
		}

		auto await_resume() {
			return std::tuple(getstatus(), numberOfBytesTransferred);
		}

	private:
		SOCKETTYPE& socket;
		WSABUF buffer;
		const SocketAddress& remoteEndPoint;

	};
}
#endif
