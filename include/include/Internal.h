#pragma once
#include "defs.h"
#include <experimental/coroutine>

namespace SL::NET {
	class AwaitableContext;
}
namespace SL::NET::INTERNAL {

#if _WIN32
	class AcceptExCreator {
	public:
		LPFN_ACCEPTEX AcceptEx_ = nullptr;
		AcceptExCreator() noexcept
		{
			auto temphandle = WSASocketW(AF_INET, SOCK_STREAM, IPPROTO_IP, NULL, 0, WSA_FLAG_OVERLAPPED);
			GUID acceptex_guid = WSAID_ACCEPTEX;
			DWORD bytes = 0;
			WSAIoctl(temphandle, SIO_GET_EXTENSION_FUNCTION_POINTER, &acceptex_guid, sizeof(acceptex_guid), &AcceptEx_, sizeof(AcceptEx_), &bytes, NULL,
				NULL);
			assert(AcceptEx_ != nullptr);
			closesocket(temphandle);
		}
	};
	class ConnectExCreator {
	public:
		LPFN_CONNECTEX ConnectEx_ = nullptr;
		ConnectExCreator() noexcept
		{
			auto temphandle = WSASocketW(AF_INET, SOCK_STREAM, IPPROTO_IP, NULL, 0, WSA_FLAG_OVERLAPPED);
			GUID guid = WSAID_CONNECTEX;
			DWORD bytes = 0;
			ConnectEx_ = nullptr;
			WSAIoctl(temphandle, SIO_GET_EXTENSION_FUNCTION_POINTER, &guid, sizeof(guid), &ConnectEx_, sizeof(ConnectEx_), &bytes, NULL, NULL);
			assert(ConnectEx_ != nullptr);
			closesocket(temphandle);
		}
	};

	auto inline win32Bind(AddressFamily family, SOCKET socket) {
		if (family == AddressFamily::IPV4) {
			sockaddr_in bindaddr = { 0 };
			bindaddr.sin_family = AF_INET;
			bindaddr.sin_addr.s_addr = INADDR_ANY;
			bindaddr.sin_port = 0;
			return ::bind(socket, (::sockaddr *)&bindaddr, sizeof(bindaddr));
		}
		else {
			sockaddr_in6 bindaddr = { 0 };
			bindaddr.sin6_family = AF_INET6;
			bindaddr.sin6_addr = in6addr_any;
			bindaddr.sin6_port = 0;
			return ::bind(socket, (::sockaddr *)&bindaddr, sizeof(bindaddr));
		}
	}

	auto inline Create(const AddressFamily &family)
	{
		int typ = SOCK_STREAM;
		SocketHandle handle = INVALID_SOCKET;
		auto errcode = StatusCode::SC_SUCCESS;
		if (family == AddressFamily::IPV4) {
			handle.value = ::socket(AF_INET, typ, 0);
		}
		else {
			handle.value = ::socket(AF_INET6, typ, 0);
		}
		if (handle.value == INVALID_SOCKET) {
			return std::tuple(TranslateError(), handle);
		}
		u_long iMode = 1;
		ioctlsocket(handle.value, FIONBIO, &iMode);
		return std::tuple(errcode, handle);
	}

#endif
	inline bool wouldBlock() noexcept
	{
#if _WIN32
		return WSAGetLastError() == WSAEWOULDBLOCK;
#else
		return errno != EAGAIN && errno != EINTR;
#endif
	}
	int inline Send(SocketHandle handle, std::byte *buf, int len) noexcept
	{
#if _WIN32
		return ::send(handle.value, reinterpret_cast<char *>(buf), static_cast<int>(len), 0);
#else
		return ::send(handle.value, buf, static_cast<int>(len), MSG_NOSIGNAL);
#endif
	}
	int inline Send(SocketHandle handle, unsigned char *buf, int len) noexcept
	{
#if _WIN32
		return ::send(handle.value, reinterpret_cast<char *>(buf), static_cast<int>(len), 0);
#else
		return ::send(handle.value, buf, static_cast<int>(len), MSG_NOSIGNAL);
#endif
	}
	int inline Recv(SocketHandle handle, std::byte *buf, int len) noexcept
	{
#if _WIN32
		return ::recv(handle.value, reinterpret_cast<char *>(buf), static_cast<int>(len), 0);
#else
		return ::recv(handle.value, buf, static_cast<int>(len), MSG_NOSIGNAL);
#endif
	}
	int inline Recv(SocketHandle handle, unsigned char *buf, int len) noexcept
	{
#if _WIN32
		return ::recv(handle.value, reinterpret_cast<char *>(buf), static_cast<int>(len), 0);
#else
		return ::recv(handle.value, buf, static_cast<int>(len), MSG_NOSIGNAL);
#endif
	}
	struct Awaiter {

#if _WIN32
		WSAOVERLAPPED Overlapped;
#endif

		Awaiter(SocketHandle h, IO_OPERATION op, AwaitableContext& context) : StatusCode_(StatusCode::SC_SUCCESS), Operation(op), SocketHandle_(h), Context(context) {
#if _WIN32
			Overlapped.Internal = Overlapped.InternalHigh = 0;
			Overlapped.Offset = Overlapped.OffsetHigh = 0;
			Overlapped.Pointer = Overlapped.hEvent = 0;
#endif  
		}
		std::experimental::coroutine_handle<> ResumeHandle;
		StatusCode StatusCode_;
		IO_OPERATION Operation;
		SocketHandle SocketHandle_;
		AwaitableContext& Context;

	};
	struct ReadAwaiter : INTERNAL::Awaiter {
		std::byte *buffer;
		int buffer_size;
		int remainingbytes;
		bool awaitready;

		void IOError() {
			StatusCode_ = TranslateError();
			buffer_size = remainingbytes = 0;
			awaitready = true;
			ResumeHandle.resume();
		}
		void IOSuccess() {
			StatusCode_ = StatusCode::SC_SUCCESS;
			awaitready = true;
			ResumeHandle.resume();
		}

		ReadAwaiter(std::byte *b, int buffersize, int bytestransfered, SocketHandle h, AwaitableContext& context)  noexcept :
			INTERNAL::Awaiter(h, IO_OPERATION::IoRead, context), buffer(b), buffer_size(buffersize), awaitready(false) {
			remainingbytes = buffersize - bytestransfered;
			awaitready = remainingbytes == 0; 
			if (bytestransfered <= 0) {
				StatusCode_ = TranslateError();
				buffer_size = 0;
				awaitready = true;
			}
		}
		constexpr auto await_ready() const {
			return awaitready;
		}
		constexpr auto await_resume() {
			return std::tuple(StatusCode_, buffer_size);
		}
		bool await_suspend(std::experimental::coroutine_handle<> h);
	};
	struct WriteAwaiter : INTERNAL::Awaiter {
		std::byte *buffer;
		int buffer_size;
		int remainingbytes;
		bool awaitready;

		void IOError() {
			StatusCode_ = TranslateError();
			buffer_size = remainingbytes = 0;
			awaitready = true;
			ResumeHandle.resume();
		}
		void IOSuccess() {
			StatusCode_ = StatusCode::SC_SUCCESS;
			awaitready = true;
			ResumeHandle.resume();
		}

		WriteAwaiter(std::byte *b, int buffersize, int bytestransfered,  SocketHandle h, AwaitableContext& context)  noexcept :
			INTERNAL::Awaiter(h, IO_OPERATION::IoWrite, context), buffer(b), buffer_size(buffersize), awaitready(false) {
			remainingbytes = buffersize - bytestransfered;
			awaitready = remainingbytes == 0;
			if (bytestransfered < 0) {
				StatusCode_ = TranslateError();
				buffer_size = 0;
				awaitready = true;
			}
		}
		constexpr auto await_ready() const {
			return awaitready;
		}
		constexpr auto await_resume() {
			return std::tuple(StatusCode_, buffer_size);
		}
		bool await_suspend(std::experimental::coroutine_handle<> h);
	};



	template <class SOCKETHANDLERTYPE>
	void setup(RW_Context &rwcontext, std::atomic<int> &iocount, IO_OPERATION op, int buffer_size, unsigned char *buffer,
		const SOCKETHANDLERTYPE &handler) noexcept
	{
		assert(!rwcontext.hasCompletionHandler()); // cannot call if there is a pending operation
		rwcontext.buffer = buffer;
		rwcontext.setRemainingBytes(buffer_size);
		rwcontext.setCompletionHandler(handler);
		rwcontext.setEvent(op);
		iocount.fetch_add(1, std::memory_order_acquire);
	}

	inline void completeio(RW_Context &rwcontext, std::atomic<int> &iocount, StatusCode code) noexcept
	{
		if (auto h(rwcontext.getCompletionHandler()); h) {
			iocount.fetch_sub(1, std::memory_order_acquire);
			h(code);
		}
	}


} // namespace SL::NET::INTERNAL
