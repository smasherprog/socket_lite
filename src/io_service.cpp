///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Lewis Baker
// Licenced under MIT license. See LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////

#include "io_service.h"
#include "utils.h"
#include <experimental/coroutine>
#include <thread>

#if _WIN32

namespace SL::Network {

	std::string win32::GetErrorMessage(unsigned long errorMessageID)
	{
		if (errorMessageID == 0)
			return std::string(); // No error message has been recorded

		LPSTR messageBuffer = nullptr;
		size_t size = FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, errorMessageID,
			MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&messageBuffer, 0, NULL);
		std::string message(messageBuffer, size);
		// Free the buffer.
		LocalFree(messageBuffer);
		return message;
	}
	unsigned long win32::GetLastError() { return ::GetLastError(); }
	void safe_handle::close() noexcept
	{
		if (shandle != nullptr && shandle != INVALID_HANDLE_VALUE) {
			::CloseHandle(shandle);
			shandle = nullptr;
		}
	}

	void wakeup(const safe_handle& h)
	{
		if (h) {
			PostQueuedCompletionStatus(h.handle(), 0, (DWORD)NULL, NULL);
		}
	}

	io_service::io_service(std::uint32_t concurrencyHint = std::thread::hardware_concurrency()) : KeepGoing(true)
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
		wakeup(IOCPHandle);
	}

	void io_service::run()
	{
		while (true) {
			DWORD numberOfBytesTransferred = 0;
			ULONG_PTR completionKey = 0;
			LPOVERLAPPED overlapped = nullptr;
			BOOL ok = ::GetQueuedCompletionStatus(IOCPHandle.handle(), &numberOfBytesTransferred, &completionKey, &overlapped, INFINITE);
			if (overlapped != nullptr) {
				DWORD errorCode = ok ? ERROR_SUCCESS : ::GetLastError();
				auto state = reinterpret_cast<overlapped_operation*>(overlapped);
				on_operation_completed(*state, errorCode, numberOfBytesTransferred);
			}
			else if (ok) {
				if (!KeepGoing) {
					wakeup(IOCPHandle);
					return;
				}
			}
			else {
				auto errorcode = ::GetLastError();
				if (errorcode != WAIT_TIMEOUT) {
					THROWEXCEPTIONWCODE(errorcode);
				}
			}
		}
	}
} // namespace SL::Network

#endif
