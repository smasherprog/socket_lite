#ifndef SL_NETWORK_IO_SERVICE
#define SL_NETWORK_IO_SERVICE

#include <cstdint> 
#include "impl.h"
#include "error_handling.h"
#include "socket_address.h"
#include "socket_io_events.h"

namespace SL::Network {
#if _WIN32
	namespace win32 {
		LPFN_DISCONNECTEX DisconnectEx_ = nullptr;
		LPFN_CONNECTEX ConnectEx_ = nullptr;
		LPFN_ACCEPTEX AcceptEx_ = nullptr;
		std::uint8_t addressBuffer[(sizeof(SOCKADDR_STORAGE) + 16) * 2] = { 0 };
	}
#endif

	template<typename IOEVENTS>class io_service {
		IOEVENTS IOEvents;
		std::uint8_t addressBuffer[(sizeof(SOCKADDR_STORAGE) + 16) * 2];
	public:

		io_service(IOEVENTS&& ioevents, std::uint32_t concurrencyHint = 1) :IOEvents(ioevents), KeepGoing(true)
		{
#if _WIN32
			WSADATA winsockData;
			if (WSAStartup(MAKEWORD(2, 2), &winsockData) == SOCKET_ERROR) {
				assert(false);
			}
			IOCPHandle = Impl::safe_handle(CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, concurrencyHint));
			if (!IOCPHandle) {
				assert(false);
			}
			Impl::SetupWindowsEvents();
#endif 
		}

		~io_service()
		{
#if _WIN32
			WSACleanup();
#endif
		}

		io_service(io_service&& other) = delete;
		io_service(const io_service& other) = delete;
		io_service& operator=(io_service&& other) = delete;
		io_service& operator=(const io_service& other) = delete;
		void run() {
			while (true) {
				DWORD numberOfBytesTransferred = 0;
				ULONG_PTR completionKey = 0;
				LPOVERLAPPED overlapped = nullptr;
				auto bSuccess = ::GetQueuedCompletionStatus(IOCPHandle.handle(), &numberOfBytesTransferred, &completionKey, &overlapped, INFINITE) == TRUE && KeepGoing;
				if (overlapped != nullptr) {
					auto state = reinterpret_cast<overlapped_operation*>(overlapped);
					auto status = bSuccess ? ERROR_SUCCESS : ::WSAGetLastError();
					auto e = Impl::TranslateError(status);
					auto originalvalue = state->exchangestatus(e);
					if (originalvalue == StatusCode::SC_PENDINGIO || originalvalue == StatusCode::SC_UNSET) {
						switch (state->OpType)
						{
						case OP_Type::OnAccept:
							{
								auto acceptstate = reinterpret_cast<accept_overlapped_operation*>(overlapped);
								if (e == StatusCode::SC_SUCCESS && ::setsockopt(acceptstate->Socket, SOL_SOCKET, SO_UPDATE_ACCEPT_CONTEXT, (const char*)&(acceptstate->ListenSocket), sizeof(SOCKET)) == SOCKET_ERROR) {
									e = Impl::TranslateError();
								}
								IOEvents.OnAccept(*this, SL::Network::socket(state->Socket, *this), e, SL::Network::socket(acceptstate->ListenSocket, *this));
								delete acceptstate;
							}
							break;
						case OP_Type::OnConnect:
							if (e == StatusCode::SC_SUCCESS && ::setsockopt(state->Socket, SOL_SOCKET, SO_UPDATE_CONNECT_CONTEXT, nullptr, 0) == SOCKET_ERROR) {
								e = Impl::TranslateError();
							}
							IOEvents.OnConnect(*this, SL::Network::socket(state->Socket, *this), e);
							delete state;
							break;
						case SL::Network::OP_Type::OnSend:
							IOEvents.OnSend(*this, SL::Network::socket(state->Socket, *this), e, numberOfBytesTransferred);
							delete state;
							break;
						case SL::Network::OP_Type::OnRead:
							IOEvents.OnRecv(*this, SL::Network::socket(state->Socket, *this), e, numberOfBytesTransferred);
							delete state;
							break;
						case SL::Network::OP_Type::OnReadFrom:
							break;
						case SL::Network::OP_Type::OnSendTo:
							break;
						default:
							break;
						}
						refcounter.decOp();
					}
					continue;
				}
				if (!KeepGoing && refcounter.getOpCount() == 0) {
					PostQueuedCompletionStatus(IOCPHandle.handle(), 0, (DWORD)NULL, NULL);
					return;
				}
			}
		}
		void stop()
		{
			KeepGoing = false;
			PostQueuedCompletionStatus(IOCPHandle.handle(), 0, (DWORD)NULL, NULL);
		}

	private:
		Impl::refcounter refcounter;
		Impl::safe_handle IOCPHandle;
#ifndef _WIN32 
		int EventWakeFd;
#endif
		bool KeepGoing;
		template<typename>friend class socket;
	};
}

#endif
