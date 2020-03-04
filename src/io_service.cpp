#include "io_service.h"
#include "socket.h"

namespace SL::Network {
#if _WIN32
	namespace win32 {
		LPFN_DISCONNECTEX DisconnectEx_ = nullptr;
		LPFN_CONNECTEX ConnectEx_ = nullptr;
		LPFN_ACCEPTEX AcceptEx_ = nullptr;
		std::uint8_t addressBuffer[(sizeof(SOCKADDR_STORAGE) + 16) * 2] = { 0 };
	}
#endif
	io_service::io_service(std::uint32_t concurrencyHint) : KeepGoing(true)
	{
#if _WIN32
		WSADATA winsockData;
		if (WSAStartup(MAKEWORD(2, 2), &winsockData) == SOCKET_ERROR) {
			assert(false);
		}
		IOCPHandle = safe_handle(CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, concurrencyHint));
		if (!IOCPHandle) {
			assert(false);
		}
		win32::SetupWindowsEvents();
#endif 
	}

	io_service::~io_service()
	{
#if _WIN32
		WSACleanup();
#endif
	}
	void io_service::stop()
	{
		KeepGoing = false;
		PostQueuedCompletionStatus(IOCPHandle.handle(), 0, (DWORD)NULL, NULL);
	}

	void io_service::run()
	{
		while (true) {
			DWORD numberOfBytesTransferred = 0;
			ULONG_PTR completionKey = 0;
			LPOVERLAPPED overlapped = nullptr;
			auto bSuccess = ::GetQueuedCompletionStatus(IOCPHandle.handle(), &numberOfBytesTransferred, &completionKey, &overlapped, INFINITE) == TRUE && KeepGoing;
			if (overlapped != nullptr) {
				auto state = reinterpret_cast<overlapped_operation*>(overlapped);
				auto status = bSuccess ? ERROR_SUCCESS : ::WSAGetLastError();
				auto e = TranslateError(status);
				auto originalvalue = state->exchangestatus(e);
				if (originalvalue == StatusCode::SC_PENDINGIO || originalvalue == StatusCode::SC_UNSET) {
					switch (state->OpType) {
					case OP_Type::Connect:
					{
						auto connectstate = reinterpret_cast<connect_overlapped_operation*>(overlapped);
						connectstate->awaitingCoroutine(e, SL::Network::socket(connectstate->socket, *this));
						delete connectstate;
					}
					break;
					case OP_Type::RW:
					{
						auto rwstate = reinterpret_cast<rw_overlapped_operation*>(overlapped);
						rwstate->awaitingCoroutine(e, numberOfBytesTransferred);
						delete rwstate;
					}
					break;
					case OP_Type::RF:
					{
						auto rwstate = reinterpret_cast<rf_overlapped_operation*>(overlapped);
						rwstate->awaitingCoroutine(e, numberOfBytesTransferred);
						delete rwstate;
					}
					break;
					case OP_Type::ST:
					{
						auto rwstate = reinterpret_cast<st_overlapped_operation*>(overlapped);
						rwstate->awaitingCoroutine(e, numberOfBytesTransferred);
						delete rwstate;
					}
					break;
					case OP_Type::Disconnect:
					{
						auto dconnectstate = reinterpret_cast<disconnect_overlapped_operation*>(overlapped);
						dconnectstate->awaitingCoroutine(e);
						delete dconnectstate;
					}
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
} // namespace SL::Network

