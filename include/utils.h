#ifndef SL_NETWORK_UTILS
#define SL_NETWORK_UTILS

#if __APPLE__
#include <experimental/optional>
#else
#include <optional>
#endif
#include <algorithm>
#include <assert.h>
#include <atomic>
#include <chrono>
#include <memory>
#include <stdint.h>
#include <string>
#include <thread> 
#include <experimental/coroutine>  
#include <vector>
#include <compare>
#include <variant>
#include <functional>

#if _WIN32 
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN 
#endif
#ifndef NOMINMAX
#define NOMINMAX 
#endif
#include <WinSock2.h>
#include <Windows.h>
#include <Ws2tcpip.h>
#include <mswsock.h>
#ifndef SOCKLEN_T
typedef int SOCKLEN_T;
#endif
#else
#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#ifndef SOCKLEN_T
typedef socklen_t SOCKLEN_T;
#endif
#endif

#ifndef INVALID_SOCKET
#define INVALID_SOCKET -1
#endif
#ifndef SOCKET_ERROR
#define SOCKET_ERROR -1
#endif


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
	class socket;
	enum class [[nodiscard]] StatusCode{ SC_UNSET, SC_CLOSED, SC_SUCCESS, SC_EAGAIN, SC_EWOULDBLOCK, SC_EBADF, SC_ECONNRESET, SC_EINTR, SC_EINVAL, SC_ENOTCONN, SC_ENOTSOCK, SC_EOPNOTSUPP, SC_ETIMEDOUT, SC_NOTSUPPORTED, SC_PENDINGIO };

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
	enum AddressFamily :int { IPV4 = AF_INET, IPV6 = AF_INET6 };
	enum SocketType :int { TCP = SOCK_STREAM, UDP = SOCK_DGRAM };
	class SocketAddress {
		::sockaddr_storage Storage;
		int Length;
		SocketType socktype;
	public:
		SocketAddress() : Storage{ 0 }, Length(0), socktype(SocketType::TCP){}
		SocketAddress(SocketAddress&& addr) noexcept : Length(addr.Length), socktype(addr.socktype)
		{
			memcpy(&Storage, &addr.Storage, addr.Length);
			addr.Length = 0;
		}
		SocketAddress(const SocketAddress& addr) noexcept : Length(addr.Length), socktype(addr.socktype) { memcpy(&Storage, &addr.Storage, addr.Length); }
		SocketAddress(::sockaddr* buffer, socklen_t len, SocketType socktype) noexcept : Length(static_cast<int>(len)), socktype(socktype)
		{
			assert(len < sizeof(Storage));
			memcpy(&Storage, buffer, len);

		}
		SocketAddress(::sockaddr_storage* buffer, socklen_t len, SocketType socktype) noexcept : Length(static_cast<int>(len)), socktype(socktype)
		{
			assert(len < sizeof(Storage));
			memcpy(&Storage, buffer, len);
		}

		[[nodiscard]] const sockaddr* getSocketAddr() const noexcept { return reinterpret_cast<const ::sockaddr*>(&Storage); }
		[[nodiscard]] int getSocketAddrLen() const noexcept { return Length; }
		[[nodiscard]] SocketType getSocketType() const noexcept { return socktype; }
		[[nodiscard]] std::string getHost() const noexcept
		{
			char str[INET_ADDRSTRLEN] = {};
			auto sockin = reinterpret_cast<const ::sockaddr_in*>(&Storage);
			inet_ntop(Storage.ss_family, &(sockin->sin_addr), str, INET_ADDRSTRLEN);
			return std::string(str);
		}
		[[nodiscard]] unsigned short getPort() const noexcept
		{
			// both ipv6 and ipv4 structs have their port in the same place!
			auto sockin = reinterpret_cast<const sockaddr_in*>(&Storage);
			return ntohs(sockin->sin_port);
		}
		[[nodiscard]] AddressFamily getFamily() const noexcept { return static_cast<AddressFamily>(Storage.ss_family); }
	};

	[[nodiscard]] inline std::vector<SocketAddress> getaddrinfo(const char* address, unsigned short port, SocketType sockettype = SocketType::TCP, AddressFamily family = AddressFamily::IPV4)
	{
		::addrinfo hints = { 0 };
		::addrinfo* result(nullptr);
		memset(&hints, 0, sizeof(addrinfo));
		hints.ai_family = family;
		hints.ai_socktype = sockettype;
		hints.ai_protocol = sockettype == SocketType::TCP ? IPPROTO_TCP : IPPROTO_UDP;
		hints.ai_flags = AI_PASSIVE; /* All interfaces */
		std::vector<SocketAddress> addrs;
		auto portstr = std::to_string(port);
		auto s = ::getaddrinfo(address, portstr.c_str(), &hints, &result);
		if (s != 0) {
			return addrs;
		}
		for (auto ptr = result; ptr != NULL; ptr = ptr->ai_next) {
			addrs.emplace_back(SocketAddress(ptr->ai_addr, static_cast<socklen_t>(ptr->ai_addrlen), sockettype));
		}
		freeaddrinfo(result);
		return addrs;
	}

#if _WIN32 
	class safe_handle {
	public:
		safe_handle() : shandle(nullptr) {}
		explicit safe_handle(void* handle) : shandle(handle) {}
		safe_handle(const safe_handle& other) = delete;
		safe_handle(safe_handle&& other) noexcept : shandle(other.shandle) { other.shandle = nullptr; }
		~safe_handle() { close(); }
		safe_handle& operator=(safe_handle handle) noexcept
		{
			swap(handle);
			return *this;
		}
		constexpr void* handle() const { return shandle; }
		constexpr operator bool() const noexcept { return shandle != nullptr; }
		/// Calls CloseHandle() and sets the handle to NULL.
		void close() noexcept
		{
			if (shandle != nullptr && shandle != INVALID_HANDLE_VALUE) {
				::CloseHandle(shandle);
				shandle = nullptr;
			}
		}

		void swap(safe_handle& other) noexcept { std::swap(shandle, other.shandle); }
		constexpr std::weak_ordering operator<=>(safe_handle const& rhs) const = default;

	private:
		void* shandle;
	};
	class refcounter {
		std::atomic<size_t> PendingOps;
	public:
		refcounter() { PendingOps = 0; }
		size_t getOpCount() const { return PendingOps.load(std::memory_order::memory_order_relaxed); }
		size_t incOp() { return PendingOps.fetch_add(1, std::memory_order::memory_order_relaxed); }
		size_t decOp() { return PendingOps.fetch_sub(1, std::memory_order::memory_order_relaxed); }
	};
	namespace win32 {
		extern LPFN_DISCONNECTEX DisconnectEx_;
		extern LPFN_CONNECTEX ConnectEx_;
		extern LPFN_ACCEPTEX AcceptEx_;
		extern std::uint8_t addressBuffer[(sizeof(SOCKADDR_STORAGE) + 16) * 2];
		inline void SetupWindowsEvents() {
			if (!DisconnectEx_) {
				auto temphandle = WSASocketW(AF_INET, SOCK_STREAM, IPPROTO_IP, NULL, 0, WSA_FLAG_OVERLAPPED);
				GUID disconnectex_guid = WSAID_DISCONNECTEX;
				DWORD bytes = 0;
				WSAIoctl(temphandle, SIO_GET_EXTENSION_FUNCTION_POINTER, &disconnectex_guid, sizeof(disconnectex_guid), &DisconnectEx_, sizeof(DisconnectEx_), &bytes, NULL,
					NULL);
				assert(DisconnectEx_ != nullptr);
				closesocket(temphandle);
			}
			if (!AcceptEx_) {
				auto temphandle = WSASocketW(AF_INET, SOCK_STREAM, IPPROTO_IP, NULL, 0, WSA_FLAG_OVERLAPPED);
				GUID acceptex_guid = WSAID_ACCEPTEX;
				DWORD bytes = 0;
				WSAIoctl(temphandle, SIO_GET_EXTENSION_FUNCTION_POINTER, &acceptex_guid, sizeof(acceptex_guid), &AcceptEx_, sizeof(AcceptEx_), &bytes, NULL,
					NULL);
				assert(AcceptEx_ != nullptr);
				closesocket(temphandle);
			}
			if (!ConnectEx_) {
				auto temphandle = WSASocketW(AF_INET, SOCK_STREAM, IPPROTO_IP, NULL, 0, WSA_FLAG_OVERLAPPED);
				GUID guid = WSAID_CONNECTEX;
				DWORD bytes = 0;
				ConnectEx_ = nullptr;
				WSAIoctl(temphandle, SIO_GET_EXTENSION_FUNCTION_POINTER, &guid, sizeof(guid), &ConnectEx_, sizeof(ConnectEx_), &bytes, NULL, NULL);
				assert(ConnectEx_ != nullptr);
				closesocket(temphandle);
			}
		}

		auto inline win32Bind(AddressFamily family, SOCKET socket) {
			if (family == AddressFamily::IPV4) {
				sockaddr_in bindaddr = { 0 };
				bindaddr.sin_family = AF_INET;
				bindaddr.sin_addr.s_addr = INADDR_ANY;
				bindaddr.sin_port = 0;
				return ::bind(socket, (::sockaddr*) & bindaddr, sizeof(bindaddr));
			}
			else {
				sockaddr_in6 bindaddr = { 0 };
				bindaddr.sin6_family = AF_INET6;
				bindaddr.sin6_addr = in6addr_any;
				bindaddr.sin6_port = 0;
				return ::bind(socket, (::sockaddr*) & bindaddr, sizeof(bindaddr));
			}
		}
	} // namespace win32

	class overlapped_operation : public WSAOVERLAPPED {
	private:
		std::atomic<StatusCode> errorCode;
	public:
		overlapped_operation() : WSAOVERLAPPED({ 0 }) {
			errorCode.store(StatusCode::SC_UNSET, std::memory_order::memory_order_relaxed);
		}
		StatusCode trysetstatus(StatusCode code, StatusCode expected) {
			auto originalexpected = expected;
			while (!errorCode.compare_exchange_weak(expected, code) && expected == originalexpected);
			return expected;
		}
		void setstatus(StatusCode code) {
			errorCode.store(code, std::memory_order::memory_order_relaxed);
		}
		StatusCode getstatus() {
			return errorCode.load(std::memory_order::memory_order_relaxed);
		}
		StatusCode exchangestatus(StatusCode code) {
			return errorCode.exchange(code, std::memory_order::memory_order_relaxed);
		}

		WSAOVERLAPPED* getOverlappedStruct() { return reinterpret_cast<WSAOVERLAPPED*>(this); }
	};
	class rw_overlapped_operation : public overlapped_operation {
	public:
		rw_overlapped_operation(std::function<void(StatusCode, size_t)>&& coro) : awaitingCoroutine(coro) {}
		std::function<void(StatusCode, size_t)> awaitingCoroutine;
	};

	class connect_overlapped_operation : public overlapped_operation {
	public:
		connect_overlapped_operation(SOCKET s, std::function<void(StatusCode, SL::Network::socket&&)>&& coro) : awaitingCoroutine(coro), socket(s) {}
		std::function<void(StatusCode, SL::Network::socket&&)> awaitingCoroutine;
		SOCKET socket;
	};

#endif
} // namespace SL::Network
#endif