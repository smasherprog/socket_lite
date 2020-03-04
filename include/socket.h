#ifndef SL_NETWORK_SOCKET
#define SL_NETWORK_SOCKET

#include "utils.h"
#include "io_service.h"
#include <tuple>

namespace SL::Network {
	class io_service;
	class socket {
	public:
		static auto create(io_service& ioSvc, SocketType sockettype = SocketType::TCP, AddressFamily family = AddressFamily::IPV4) {
			auto protocol = sockettype == SocketType::TCP ? IPPROTO_TCP : IPPROTO_UDP;

			auto socketHandle = ::socket(family, sockettype, protocol);
			if (socketHandle == INVALID_SOCKET) {
				return std::tuple(TranslateError(), socket(ioSvc));
			}

			if (sockettype == SocketType::TCP) {
				// Turn off linger so that the destructor doesn't block while closing
				// the socket or silently continue to flush remaining data in the
				// background after ::closesocket() is called, which could fail and
				// we'd never know about it.
				// We expect clients to call Disconnect() or use CloseSend() to cleanly
				// shut-down connections instead.
				BOOL value = TRUE;
				if (::setsockopt(socketHandle, SOL_SOCKET, SO_DONTLINGER, reinterpret_cast<const char*>(&value), sizeof(value)) == SOCKET_ERROR) {
					closesocket(socketHandle);
					return std::tuple(TranslateError(), socket(ioSvc));
				}
			}
			return std::tuple(StatusCode::SC_SUCCESS, std::move(socket(socketHandle, ioSvc)));
		}
		socket(socket&& other) noexcept : shandle(std::exchange(other.shandle, INVALID_SOCKET)), ioservice(other.ioservice) {}
		~socket() { close(); }
		[[nodiscard]] auto local_endpoint() const
		{
			sockaddr_storage addr = { 0 };
			socklen_t len = sizeof(addr);
			if (::getsockname(shandle, (::sockaddr*) & addr, &len) == 0) {
				int type;
				int length = sizeof(int);
				getsockopt(shandle, SOL_SOCKET, SO_TYPE, (char*)&type, &length);
				std::tuple(StatusCode::SC_SUCCESS, SocketAddress(&addr, len, type == IPPROTO_TCP ? SocketType::TCP : SocketType::UDP));
			}
			return std::tuple(TranslateError(), SocketAddress());
		}

		[[nodiscard]] auto remote_endpoint() const
		{
			sockaddr_storage addr = { 0 };
			socklen_t len = sizeof(addr);
			if (::getpeername(shandle, (::sockaddr*) & addr, &len) == 0) {
				int type;
				int length = sizeof(int);
				getsockopt(shandle, SOL_SOCKET, SO_TYPE, (char*)&type, &length);
				std::tuple(StatusCode::SC_SUCCESS, SocketAddress(&addr, len, type == IPPROTO_TCP ? SocketType::TCP : SocketType::UDP));
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
		void close() {
			if (shandle != INVALID_SOCKET) {
				auto s = shandle;
				shandle = INVALID_SOCKET;
				::CancelIoEx((HANDLE)s, NULL);
				::closesocket(s);
			}
		}

		template<class T>static auto connect(io_service& context, const SocketAddress& remoteEndPoint, T&& cb) noexcept {
			auto [statuscode, s] = SL::Network::socket::create(context, remoteEndPoint.getSocketType(), remoteEndPoint.getFamily());
			if (statuscode != StatusCode::SC_SUCCESS) {
				return  std::tuple(statuscode, std::move(s));
			}
			auto handle = s.shandle;
			if (::CreateIoCompletionPort((HANDLE)handle, context.getHandle(), handle, 0) == NULL) {
				return  std::tuple(TranslateError(), std::move(s));
			}
			if (win32::win32Bind(remoteEndPoint.getFamily(), handle) == SOCKET_ERROR) {
				return  std::tuple(TranslateError(), std::move(s));
			}
			if (SetFileCompletionNotificationModes((HANDLE)handle, FILE_SKIP_COMPLETION_PORT_ON_SUCCESS) == FALSE) {
				return  std::tuple(TranslateError(), std::move(s));
			}
			auto overlapped = new connect_overlapped_operation(handle, cb);
			context.getRefCounter().incOp();
			DWORD transferedbytes = 0;
			auto e = StatusCode::SC_SUCCESS;
			if (win32::ConnectEx_(handle, remoteEndPoint.getSocketAddr(), remoteEndPoint.getSocketAddrLen(), 0, 0, &transferedbytes, overlapped->getOverlappedStruct()) == FALSE) {
				e = TranslateError();
				auto originalvalue = overlapped->trysetstatus(e, StatusCode::SC_UNSET);
				if (originalvalue == StatusCode::SC_UNSET) {///successfully change from unset to my value and a real error has occured, no iocp will be fired
					if (e != StatusCode::SC_PENDINGIO) {
						context.getRefCounter().decOp();
						delete overlapped;
					}
				}
				////otherwise, the op is pending and ioservice will handle this
			}
			else {
				if (::setsockopt(handle, SOL_SOCKET, SO_UPDATE_CONNECT_CONTEXT, nullptr, 0) == SOCKET_ERROR) {
					e = TranslateError();
				}
				context.getRefCounter().decOp();
				delete overlapped;
			}
			return std::tuple(e, std::move(s));
		}

		template<class T>auto disconnect(T&& cb) noexcept {
			auto overlapped = new disconnect_overlapped_operation(shandle, cb);
			ioservice.getRefCounter().incOp();
			DWORD transferedbytes = 0;
			auto e = StatusCode::SC_SUCCESS;
			if (win32::DisconnectEx_(shandle, overlapped->getOverlappedStruct(), 0, 0) == FALSE) {
				e = TranslateError();
				auto originalvalue = overlapped->trysetstatus(e, StatusCode::SC_UNSET);
				if (originalvalue == StatusCode::SC_UNSET) {///successfully change from unset to my value and a real error has occured, no iocp will be fired
					if (e != StatusCode::SC_PENDINGIO) {
						ioservice.getRefCounter().decOp();
						delete overlapped;
					}
				}
				////otherwise, the op is pending and ioservice will handle this
			}
			else {
				ioservice.getRefCounter().decOp();
				delete overlapped;
			}
			return e;
		}
		template<class T>auto send(std::byte* buffer, std::size_t size, T&& cb) noexcept {
			auto overlapped = new rw_overlapped_operation(shandle, cb);
			ioservice.getRefCounter().incOp();
			overlapped->wsabuf.buf = reinterpret_cast<char*>(buffer);
			overlapped->wsabuf.len = static_cast<decltype(wsabuf.len)>(size);
			DWORD transferedbytes = 0;
			auto e = StatusCode::SC_SUCCESS;
			if (::WSASend(shandle, &(overlapped->wsabuf), 1, &transferedbytes, 0, overlapped->getOverlappedStruct(), nullptr) == SOCKET_ERROR) {
				e = TranslateError();
				auto originalvalue = overlapped->trysetstatus(e, StatusCode::SC_UNSET);
				if (originalvalue == StatusCode::SC_UNSET) {///successfully change from unset to my value and a real error has occured, no iocp will be fired
					if (e != StatusCode::SC_PENDINGIO) {
						ioservice.getRefCounter().decOp();
						delete overlapped;
					}
				}
			}
			else {
				ioservice.getRefCounter().decOp();
				delete overlapped;
			}
			return std::tuple(e, numberOfBytesTransferred);
		}
		template<class T>auto send_to(const SocketAddress& remoteEndPoint, std::byte* buffer, std::size_t size, T&& cb) noexcept {
			auto overlapped = new st_overlapped_operation(shandle, cb);
			overlapped->remoteEndPoint = remoteEndPoint;
			ioservice.getRefCounter().incOp();
			overlapped->wsabuf.buf = reinterpret_cast<char*>(buffer);
			overlapped->wsabuf.len = static_cast<decltype(wsabuf.len)>(size);
			DWORD numberOfBytesTransferred(0), dwFlags(0);
			auto e = StatusCode::SC_SUCCESS;
			SocketAddress sa;
			if (::WSASendTo(shandle, &(overlapped->wsabuf), 1, &numberOfBytesTransferred, &dwFlags, overlapped->remoteEndPoint.getSocketAddr(), overlapped->remoteEndPoint.getSocketAddrLen(), overlapped->getOverlappedStruct(), nullptr) == SOCKET_ERROR) {
				e = TranslateError();
				auto originalvalue = overlapped->trysetstatus(e, StatusCode::SC_UNSET);
				if (originalvalue == StatusCode::SC_UNSET) {///successfully change from unset to my value and a real error has occured, no iocp will be fired
					if (e != StatusCode::SC_PENDINGIO) {
						ioservice.getRefCounter().decOp();
						delete overlapped;
					}
				}
			}
			else {
				ioservice.getRefCounter().decOp(); 
				delete overlapped;
			}
			return std::tuple(e, numberOfBytesTransferred);
		}
		template<class T>auto recv(std::byte* buffer, std::size_t size, T&& cb) noexcept {
			auto overlapped = new rw_overlapped_operation(shandle, cb);
			ioservice.getRefCounter().incOp();
			overlapped->wsabuf.buf = reinterpret_cast<char*>(buffer);
			overlapped->wsabuf.len = static_cast<decltype(wsabuf.len)>(size);
			DWORD dwSendNumBytes(0), dwFlags(0);
			auto e = StatusCode::SC_SUCCESS;
			if (::WSARecv(shandle, &(overlapped->wsabuf), 1, &dwSendNumBytes, &dwFlags, overlapped->getOverlappedStruct(), nullptr) == SOCKET_ERROR) {
				e = TranslateError();
				auto originalvalue = overlapped->trysetstatus(e, StatusCode::SC_UNSET);
				if (originalvalue == StatusCode::SC_UNSET) {///successfully change from unset to my value and a real error has occured, no iocp will be fired
					if (e != StatusCode::SC_PENDINGIO) {
						ioservice.getRefCounter().decOp();
						delete overlapped;
					}
				}
			}
			else {
				ioservice.getRefCounter().decOp();
				delete overlapped;
			}
			return std::tuple(e, dwSendNumBytes);
		}
		template<class T>auto recv_from(std::byte* buffer, std::size_t size, T&& cb) noexcept {
			auto overlapped = new rf_overlapped_operation(shandle, cb);
			ioservice.getRefCounter().incOp();
			overlapped->wsabuf.buf = reinterpret_cast<char*>(buffer);
			overlapped->wsabuf.len = static_cast<decltype(wsabuf.len)>(size);
			DWORD numberOfBytesTransferred(0), dwFlags(0);
			auto e = StatusCode::SC_SUCCESS;
			SocketAddress sa;
			if (::WSARecvFrom(shandle, &(overlapped->wsabuf), 1, &numberOfBytesTransferred, &dwFlags, reinterpret_cast<sockaddr*>(&(overlapped->storage)), &(overlapped->socklen), overlapped->getOverlappedStruct(), nullptr) == SOCKET_ERROR) {
				e = TranslateError();
				auto originalvalue = overlapped->trysetstatus(e, StatusCode::SC_UNSET);
				if (originalvalue == StatusCode::SC_UNSET) {///successfully change from unset to my value and a real error has occured, no iocp will be fired
					if (e != StatusCode::SC_PENDINGIO) {
						ioservice.getRefCounter().decOp();
						delete overlapped;
					}
				}
			}
			else {
				ioservice.getRefCounter().decOp();
				sa = std::move(SocketAddress(overlapped->storage, socklen));
				delete overlapped;
			}
			return std::tuple(e, sa, numberOfBytesTransferred);
		}
		/*
		[[nodiscard]] auto recv_from(std::byte* buffer, std::size_t size) noexcept { return socket_recv_from_operation<socket>(*this, buffer, size); }
		[[nodiscard]] auto send_to(const SocketAddress& destination, std::byte* buffer, std::size_t size) noexcept { return socket_send_to_operation<socket>(*this, destination, buffer, size); }*/

		SOCKET native_handle() const { return shandle; }

		explicit socket(SOCKET handle, io_service& ioSvc) noexcept : shandle(handle), ioservice(ioSvc) {}
	private:
		template<typename>friend class async_acceptor;
#if _WIN32
		explicit socket(io_service& ioSvc) noexcept : shandle(INVALID_SOCKET), ioservice(ioSvc) {}
		SOCKET shandle;
		io_service& ioservice;
#endif
	};
} // namespace SL::Network

#endif
