#ifndef SL_NETWORK_SOCKET_ACCEPT
#define SL_NETWORK_SOCKET_ACCEPT
#include "utils.h" 

namespace SL::Network {

	template<class SOCKETTYPE> class socket_accept_operation : public overlapped_operation {
	public:
		socket_accept_operation(SOCKETTYPE& listeningSocket, SOCKETTYPE& acceptingSocket) noexcept : listeningSocket(listeningSocket), acceptingSocket(acceptingSocket) {
			listeningSocket.get_ioservice().incOp();
		}	
		socket_accept_operation(socket_accept_operation&& other) noexcept : listeningSocket(other.listeningSocket), acceptingSocket(other.acceptingSocket), addressBuffer(other.addressBuffer){}
		~socket_accept_operation() {
			listeningSocket.get_ioservice().decOp();
		}

		auto await_ready() noexcept
		{
			static win32::AcceptExCreator Acceptor;
			if (::CreateIoCompletionPort((HANDLE)acceptingSocket.native_handle(), acceptingSocket.get_ioservice().getHandle(), acceptingSocket.native_handle(), 0) == NULL) {
				acceptingSocket.close();
				setstatus(TranslateError());
				return true;
			}
			if (SetFileCompletionNotificationModes((HANDLE)acceptingSocket.native_handle(), FILE_SKIP_COMPLETION_PORT_ON_SUCCESS) == FALSE) {
				acceptingSocket.close();
				setstatus(TranslateError());
				return true;
			}

			DWORD transferedbytes = 0;
			if (Acceptor.AcceptEx_(listeningSocket.native_handle(), acceptingSocket.native_handle(), addressBuffer, 0, sizeof(SOCKADDR_STORAGE) + 16, sizeof(SOCKADDR_STORAGE) + 16, &transferedbytes, getOverlappedStruct()) == FALSE) {
				auto e = TranslateError();
				auto originalvalue = trysetstatus(e, StatusCode::SC_UNSET);
				if (originalvalue == StatusCode::SC_UNSET) {///successfully change from unset to the erro
					return e != StatusCode::SC_PENDINGIO1;
				}
			}

			return true;
		}

		auto await_resume()
		{
			auto errorcode = getstatus();
			if (errorcode != StatusCode::SC_SUCCESS) {
				return errorcode;
			}

			SOCKET listenSocket = listeningSocket.native_handle();
			if (::setsockopt(acceptingSocket.native_handle(), SOL_SOCKET, SO_UPDATE_ACCEPT_CONTEXT, (const char*)&listenSocket, sizeof(SOCKET)) == SOCKET_ERROR) {
				errorcode = TranslateError();
			}
			return errorcode;
		}

	private:

		SOCKETTYPE& listeningSocket;
		SOCKETTYPE& acceptingSocket;
		std::uint8_t addressBuffer[(sizeof(SOCKADDR_STORAGE) + 16) * 2];
	};
} // namespace SL::Network

#endif
