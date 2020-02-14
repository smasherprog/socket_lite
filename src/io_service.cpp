///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Lewis Baker
// Licenced under MIT license. See LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////

#include "io_service.h" 
#include "ip_endpoint.h"
#include "utils.h" 
#include <thread> 
#include <experimental/coroutine>

#if _WIN32
# ifndef WIN32_LEAN_AND_MEAN
#  define WIN32_LEAN_AND_MEAN
# endif
# ifndef NOMINMAX
#  define NOMINMAX
# endif
# include <WinSock2.h>
# include <WS2tcpip.h>
# include <MSWSock.h>
# include <Windows.h>
#endif

namespace SL::Network {
	std::string win32::GetErrorMessage(unsigned long errorMessageID) {
		if (errorMessageID == 0)
			return std::string(); //No error message has been recorded

		LPSTR messageBuffer = nullptr;
		size_t size = FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, errorMessageID, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&messageBuffer, 0, NULL);
		std::string message(messageBuffer, size);
		//Free the buffer.
		LocalFree(messageBuffer);
		return message;
	}
	unsigned long win32::GetLastError() { return ::GetLastError(); }
	void win32::safe_handle::close() noexcept
	{
		if (m_handle != nullptr && m_handle != INVALID_HANDLE_VALUE)
		{
			::CloseHandle(m_handle);
			m_handle = nullptr;
		}
	}

	void wakeup(const win32::safe_handle& h)
	{
		if (h) {
			PostQueuedCompletionStatus(h.handle, 0, (DWORD)NULL, NULL);
		}
	}

	io_service::io_service(std::uint32_t concurrencyHint = std::thread::hardware_concurrency()) : KeepGoing(true)
	{
#if _WIN32
		WSADATA winsockData;
		if (WSAStartup(MAKEWORD(2, 2), &winsockData) == SOCKET_ERROR) {
			THROWEXCEPTION
		}
		IOCPHandle = win32::safe_handle(CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, concurrencyHint));
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
	void io_service::stop() {
		KeepGoing = false;
		wakeup(IOCPHandle);
	}

	void io_service::run() {
		while (true)
		{
			DWORD numberOfBytesTransferred = 0;
			ULONG_PTR completionKey = 0;
			LPOVERLAPPED overlapped = nullptr;
			BOOL ok = ::GetQueuedCompletionStatus(IOCPHandle.handle(), &numberOfBytesTransferred, &completionKey, &overlapped, INFINITE);
			if (overlapped != nullptr)
			{
				DWORD errorCode = ok ? ERROR_SUCCESS : ::GetLastError();
				auto* state = reinterpret_cast<win32::overlapped*>(overlapped);
				on_operation_completed(state, errorCode, numberOfBytesTransferred);
			}
			else if (ok) {
				if (!KeepGoing) {
					wakeup(IOCPHandle);
					return;
				}
			}
			else {
				auto errorcode = ::GetLastError();
				if (errorcode != WAIT_TIMEOUT)
				{
					throw std::system_error(errorcode, std::system_category(), SL::Network::win32::GetErrorMessage(errorcode));
				}
			}
		}
	}
}

