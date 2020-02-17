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
#include <system_error>
#include <vector>
#include <compare>

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


namespace SL::Network {

	enum AddressFamily :int { IPV4 = AF_INET, IPV6 = AF_INET6, IPANY = AF_UNSPEC };
	enum SocketType :int { TCP = SOCK_STREAM, UDP = SOCK_DGRAM };
	class SocketAddress {
		::sockaddr_storage Storage;
		int Length;

	public:
		SocketAddress() : Storage{ 0 }, Length(0){}
		SocketAddress(SocketAddress&& addr) noexcept : Length(addr.Length)
		{
			memcpy(&Storage, &addr.Storage, addr.Length);
			addr.Length = 0;
		}
		SocketAddress(const SocketAddress& addr) noexcept : Length(addr.Length) { memcpy(&Storage, &addr.Storage, addr.Length); }
		SocketAddress(::sockaddr* buffer, socklen_t len) : Length(static_cast<int>(len))
		{
			assert(len < sizeof(Storage));
			memcpy(&Storage, buffer, len);

		}
		SocketAddress(::sockaddr_storage* buffer, socklen_t len) noexcept
		{
			assert(len < sizeof(Storage));
			memcpy(&Storage, buffer, len);
			Length = static_cast<int>(len);
		}

		[[nodiscard]] const sockaddr* getSocketAddr() const noexcept { return reinterpret_cast<const ::sockaddr*>(&Storage); }
		[[nodiscard]] int getSocketAddrLen() const noexcept { return Length; }
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

	[[nodiscard]] inline std::vector<SocketAddress> getaddrinfo(const char* address, unsigned short port, SocketType sockettype = SocketType::TCP, AddressFamily family = AddressFamily::IPANY)
	{
		::addrinfo hints = { 0 };
		::addrinfo* result(nullptr);
		memset(&hints, 0, sizeof(addrinfo));
		hints.ai_family = family;
		hints.ai_socktype = sockettype; /* We want a TCP socket */
		hints.ai_protocol = sockettype == SocketType::TCP ? IPPROTO_TCP : IPPROTO_UDP;
		hints.ai_flags = AI_PASSIVE; /* All interfaces */
		std::vector<SocketAddress> addrs;
		auto portstr = std::to_string(port);
		auto s = ::getaddrinfo(address, portstr.c_str(), &hints, &result);
		if (s != 0) {
			return addrs;
		}
		for (auto ptr = result; ptr != NULL; ptr = ptr->ai_next) {
			addrs.emplace_back(SocketAddress(ptr->ai_addr, static_cast<socklen_t>(ptr->ai_addrlen)));
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
		void close() noexcept;

		void swap(safe_handle& other) noexcept { std::swap(shandle, other.shandle); }
		constexpr std::weak_ordering operator<=>(safe_handle const& rhs) const = default;

	private:
		void* shandle;
	};
	namespace win32 {
		class DisconnectExCreator {
		public:
			LPFN_DISCONNECTEX DisconnectEx_ = nullptr;
			DisconnectExCreator() noexcept
			{
				auto temphandle = WSASocketW(AF_INET, SOCK_STREAM, IPPROTO_IP, NULL, 0, WSA_FLAG_OVERLAPPED);
				GUID disconnectex_guid = WSAID_DISCONNECTEX;
				DWORD bytes = 0;
				WSAIoctl(temphandle, SIO_GET_EXTENSION_FUNCTION_POINTER, &disconnectex_guid, sizeof(disconnectex_guid), &DisconnectEx_, sizeof(DisconnectEx_), &bytes, NULL,
					NULL);
				assert(DisconnectEx_ != nullptr);
				closesocket(temphandle);
			}
		};

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

		std::string GetErrorMessage(DWORD errorMessageID);
		DWORD GetLastError();
	} // namespace win32

	enum class [[nodiscard]] StatusCode{
   SC_CLOSED, SC_SUCCESS, SC_EAGAIN, SC_EWOULDBLOCK, SC_EBADF, SC_ECONNRESET, SC_EINTR, SC_EINVAL, SC_ENOTCONN, SC_ENOTSOCK, SC_EOPNOTSUPP, SC_ETIMEDOUT,
		SC_NOTSUPPORTED, SC_PENDINGIO,
	};
#if _WIN32

	static StatusCode TranslateError(DWORD errcode)
	{
		switch (errcode) {
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
	static StatusCode TranslateError() {
		return TranslateError(WSAGetLastError());
	}

#else

	static StatusCode TranslateError(int errcode)
	{
		switch (errorcode) {
		default:
			return StatusCode::SC_CLOSED;
		};
	}
	static StatusCode TranslateError() {
		return TranslateError(errno);
	}
#endif

	struct overlapped_operation : public WSAOVERLAPPED {
	public:
		WSAOVERLAPPED* getOverlappedStruct() { return reinterpret_cast<WSAOVERLAPPED*>(this); }
		StatusCode errorCode;
		DWORD numberOfBytesTransferred;
		std::experimental::coroutine_handle<> awaitingCoroutine;
	};

	static void on_operation_completed(overlapped_operation& ioState, DWORD errorCode, DWORD numberOfBytesTransferred) noexcept
	{
		ioState.errorCode = TranslateError(errorCode);
		ioState.numberOfBytesTransferred = numberOfBytesTransferred;
		ioState.awaitingCoroutine.resume();
	}

#endif
} // namespace SL::Network
#if _WIN32
#define THROWEXCEPTION                                                                                                                               \
    auto errorcode = SL::Network::win32::GetLastError();                                                                                             \
    throw std::system_error(errorcode, std::system_category(), SL::Network::win32::GetErrorMessage(errorcode));
#endif
#define THROWEXCEPTIONWCODE(e) throw std::system_error(e, std::system_category(), SL::Network::win32::GetErrorMessage(e));
#endif