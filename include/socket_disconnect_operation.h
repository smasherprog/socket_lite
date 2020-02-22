#ifndef SL_NETWORK_SOCKET_DISCONNECT_OP
#define SL_NETWORK_SOCKET_DISCONNECT_OP

namespace SL::Network {
	class socket;

	template<class SOCKETTYPE> class socket_disconnect_operation : public overlapped_operation {
	public:

		socket_disconnect_operation(SOCKETTYPE& socket) noexcept : socket(socket) {
			socket.get_ioservice().incOp();
		}
		socket_disconnect_operation(socket_disconnect_operation&& other) noexcept : socket(other.socket) {}
		~socket_disconnect_operation() {
			socket.get_ioservice().decOp();
		}

		auto await_ready() noexcept
		{ 
			DWORD bytesSent = 0;  
			if (win32::DisconnectEx_(socket.native_handle(), getOverlappedStruct(), 0, 0) == FALSE) {
				errorCode = TranslateError();
				return errorCode != StatusCode::SC_PENDINGIO1;
			}

			return true;
		}
		auto await_resume() {
			if (errorCode != StatusCode::SC_SUCCESS) {
				return errorCode;
			}

			return StatusCode::SC_SUCCESS
		}

	private:
		SOCKETTYPE& socket;
	};
}
#endif
