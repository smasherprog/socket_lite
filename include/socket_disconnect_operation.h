#ifndef SL_NETWORK_SOCKET_DISCONNECT_OP
#define SL_NETWORK_SOCKET_DISCONNECT_OP

namespace SL::Network {
	class socket;

	template<class SOCKETTYPE> class socket_disconnect_operation : public overlapped_operation {
	public:

		socket_disconnect_operation(SOCKETTYPE& socket) noexcept : socket(socket), overlapped_operation({ 0 }) {
			socket.get_ioservice().incOp();
		}
		~socket_disconnect_operation() {
			socket.get_ioservice().decOp();
		}
		auto await_suspend(std::experimental::coroutine_handle<> coro) { awaitingCoroutine = coro; }
		auto await_ready() noexcept
		{
			static win32::DisconnectExCreator Disconnector;
			DWORD bytesSent = 0;  
			if (Disconnector.DisconnectEx_(socket.native_handle(), getOverlappedStruct(), 0, 0) == FALSE) {
				errorCode = TranslateError();
				return errorCode != StatusCode::SC_PENDINGIO;
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
