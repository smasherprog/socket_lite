#ifndef SL_NETWORK_ERROR_HANDLING
#define SL_NETWORK_ERROR_HANDLING

#include "impl.h"

namespace SL::Network::win32 {
	inline std::string GetErrorMessage(unsigned long errorMessageID)
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
};

namespace SL::Network {
	enum class [[nodiscard]] StatusCode{ SC_UNSET, SC_CLOSED, SC_SUCCESS, SC_EAGAIN, SC_EWOULDBLOCK, SC_EBADF, SC_ECONNRESET, SC_EINTR, SC_EINVAL, SC_ENOTCONN, SC_ENOTSOCK, SC_EOPNOTSUPP, SC_ETIMEDOUT, SC_NOTSUPPORTED, SC_PENDINGIO };
}
namespace SL::Network::Impl {
#if _WIN32

	inline StatusCode TranslateError(int errcode)
	{
		if (errcode != 0 && errcode != ERROR_IO_PENDING) {
#if _DEBUG
			auto err = win32::GetErrorMessage(errcode);
			printf(err.c_str());
#endif
		}
		switch (errcode) {
		case ERROR_SUCCESS:
			return StatusCode::SC_SUCCESS;
		case WSAECONNRESET:
			return StatusCode::SC_ECONNRESET;
		case WSAETIMEDOUT:
		case WSAECONNABORTED:
			return StatusCode::SC_ETIMEDOUT;
		case WSAEWOULDBLOCK:
			return StatusCode::SC_EWOULDBLOCK;
		case ERROR_IO_PENDING:
			return StatusCode::SC_PENDINGIO;
		default:
			return StatusCode::SC_CLOSED;
		};
	}

	inline StatusCode TranslateError() {
		return TranslateError(::WSAGetLastError());
	}

#else

	inline StatusCode TranslateError(int errcode)
	{
		switch (errorcode) {
		default:
			return StatusCode::SC_CLOSED;
		};
	}
	inline StatusCode TranslateError() {
		return TranslateError(errno);
	}
#endif
}
#endif