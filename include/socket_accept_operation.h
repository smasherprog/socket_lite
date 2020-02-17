#ifndef SL_NETWORK_SOCKET_ACCEPT
#define SL_NETWORK_SOCKET_ACCEPT
#include "utils.h" 

namespace SL::Network {

	template<class SOCKETTYPE> class socket_accept_operation : public overlapped_operation {
	public:
		socket_accept_operation(SOCKETTYPE& listeningSocket, SOCKETTYPE& acceptingSocket) noexcept : listeningSocket(listeningSocket), acceptingSocket(acceptingSocket) {}
		auto await_suspend(std::experimental::coroutine_handle<> coro) { awaitingCoroutine = coro; }
		auto socket_accept_operation::await_ready() noexcept
		{
			static win32::AcceptExCreator Acceptor;
			DWORD bytesReceived = 0;
			if (Acceptor.AcceptEx_(listeningSocket.native_handle(), acceptingSocket.native_handle(), addressBuffer, 0, sizeof(addressBuffer) / 2, sizeof(addressBuffer) / 2, &bytesReceived, getOverlappedStruct()) == FALSE) {
				errorCode = TranslateError();
				return errorCode != StatusCode::SC_PENDINGIO;
			}

			return true;
		}

		auto socket_accept_operation::await_resume()
		{
			if (errorCode != StatusCode::SC_SUCCESS) {
				return errorCode;
			}

			SOCKET listenSocket = listeningSocket.native_handle();
			if (::setsockopt(acceptingSocket.native_handle(), SOL_SOCKET, SO_UPDATE_ACCEPT_CONTEXT, (const char*)&listenSocket, sizeof(SOCKET)) == SOCKET_ERROR) {
				errorCode = TranslateError();
			}
			return errorCode;
		}

	private:

		SOCKETTYPE& listeningSocket;
		SOCKETTYPE& acceptingSocket;
		std::uint8_t addressBuffer[88];
	};
} // namespace SL::Network

#endif
