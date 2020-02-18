#ifndef SL_NETWORK_SOCKET
#define SL_NETWORK_SOCKET

#include "socket_accept_operation.h"
#include "socket_connect_operation.h"
#include "socket_disconnect_operation.h"
#include "socket_recv_from_operation.h"
#include "socket_recv_operation.h"
#include "socket_send_operation.h"
#include "socket_send_to_operation.h"

namespace SL::Network {
	class io_service;

	class socket {
	public:
		static std::tuple<StatusCode, socket>  create(io_service& ioSvc, SocketType sockettype = SocketType::TCP, AddressFamily family = AddressFamily::IPV4);

		socket(socket&& other) noexcept : shandle(std::exchange(other.shandle, INVALID_SOCKET)) {}
		~socket()
		{
			if (shandle != INVALID_SOCKET) {
				::closesocket(shandle);
			}
		}

		socket& operator=(socket&& other) noexcept
		{
			auto handle = std::exchange(other.shandle, INVALID_SOCKET);
			if (shandle != INVALID_SOCKET) {
				::closesocket(shandle);
			}

			shandle = handle;
			return *this;
		}

		[[nodiscard]] auto local_endpoint() const
		{
			sockaddr_storage addr = { 0 };
			socklen_t len = sizeof(addr);
			if (::getsockname(shandle, (::sockaddr*) & addr, &len) == 0) {
				std::tuple(StatusCode::SC_SUCCESS, SocketAddress(&addr, len));
			}
			return std::tuple(TranslateError(), SocketAddress());
		}

		[[nodiscard]] auto remote_endpoint() const
		{
			sockaddr_storage addr = { 0 };
			socklen_t len = sizeof(addr);
			if (::getpeername(shandle, (::sockaddr*) & addr, &len) == 0) {
				std::tuple(StatusCode::SC_SUCCESS, SocketAddress(&addr, len));
			}
			return std::tuple(TranslateError(), SocketAddress());
		}

		auto bind(const SocketAddress& localEndPoint)
		{
			if (::bind(shandle, localEndPoint.getSocketAddr(), localEndPoint.getSocketAddrLen()) == SOCKET_ERROR) {
				return TranslateError();
			}
			return StatusCode::SC_SUCCESS;
		}

		auto listen(std::uint32_t backlog)
		{
			if (backlog > SOMAXCONN) {
				backlog = SOMAXCONN;
			}

			if (::listen(shandle, (int)backlog) != 0) {
				return TranslateError();
			}
			return StatusCode::SC_SUCCESS;
		}
		auto listen() { return listen(SOMAXCONN); }

		[[nodiscard]] auto connect(const SocketAddress& remoteEndPoint) noexcept { return socket_connect_operation<socket>(*this, remoteEndPoint); }
		[[nodiscard]] auto accept(socket& acceptingSocket) noexcept { return socket_accept_operation<socket>(*this, acceptingSocket); }
		[[nodiscard]] auto disconnect() noexcept { return socket_disconnect_operation<socket>(*this); }
		[[nodiscard]] auto send(std::byte* buffer, std::size_t size) noexcept { return socket_send_operation<socket>(*this, buffer, size); }
		[[nodiscard]] auto recv(std::byte* buffer, std::size_t size) noexcept { return socket_recv_operation<socket>(*this, buffer, size); }
		[[nodiscard]] auto recv_from(std::byte* buffer, std::size_t size) noexcept { return socket_recv_from_operation<socket>(*this, buffer, size); }
		[[nodiscard]] auto send_to(const SocketAddress& destination, std::byte* buffer, std::size_t size) noexcept { return socket_send_to_operation<socket>(*this, destination, buffer, size); }

		SOCKET native_handle() const { return shandle; }

	private:
#if _WIN32
		explicit socket(SOCKET handle) noexcept : shandle(handle) {}
		explicit socket() noexcept : shandle(INVALID_SOCKET) {}
		SOCKET shandle;
#endif
	};
} // namespace SL::Network

#endif
