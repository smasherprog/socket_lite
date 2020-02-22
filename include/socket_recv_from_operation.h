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
			socket.get_ioservice().incOp();
		}
		~socket_recv_from_operation() {
			socket.get_ioservice().decOp();
		}

		auto await_ready() noexcept
		{
			numberOfBytesTransferred = 0;
			DWORD flags = 0; 
			if (::WSARecvFrom(m_socket.native_handle(), &buffer, 1, &numberOfBytesTransferred, &flags, reinterpret_cast<sockaddr*>(&storage), &socklen, getOverlappedStruct(), nullptr) == SOCKET_ERROR) {
			 
				return getstatus() != StatusCode::SC_PENDINGIO1;
			}

			return true;
		}

		auto await_resume() {
			return std::tuple(getstatus(), SocketAddress(storage, socklen) numberOfBytesTransferred);
		}

	private:
		SOCKETTYPE& socket;
		WSABUF buffer;
		DWORD socklen = 0;
		::sockaddr_storage storage;
	};
}
#endif
