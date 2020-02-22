#include "io_service.h"

#if _WIN32

namespace SL::Network {
	io_service::io_service(std::uint32_t concurrencyHint) : KeepGoing(true)
	{
#if _WIN32
		WSADATA winsockData;
		if (WSAStartup(MAKEWORD(2, 2), &winsockData) == SOCKET_ERROR) {
			THROWEXCEPTION
		}
		IOCPHandle = safe_handle(CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, concurrencyHint));
		if (!IOCPHandle) {
			THROWEXCEPTION
		}
		PendingOps = 0;
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
				if (bSuccess) {
					on_operation_completed(*state, ERROR_SUCCESS, numberOfBytesTransferred);
				}
				else {
					on_operation_completed(*state, ::WSAGetLastError(), numberOfBytesTransferred);
				}
				continue;
			} 
			if (!KeepGoing && PendingOps.load(std::memory_order::memory_order_relaxed) == 0) {
				PostQueuedCompletionStatus(IOCPHandle.handle(), 0, (DWORD)NULL, NULL);
				return;
			}
		}
	}
} // namespace SL::Network

#endif
