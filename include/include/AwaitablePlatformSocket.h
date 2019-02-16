#pragma once
#include "AwaitableContext.h"
#include "Internal.h"
#include "PlatformSocket.h"
#include <experimental/coroutine> 
#include <ciso646>

namespace SL::NET {
	class AwaitablePlatformSocket : public PlatformSocket {
		inline static AwaitableContext Context_;
	public:
		AwaitablePlatformSocket() noexcept {}
		// AwaitablePlatformSocket(SocketHandle h) noexcept : PlatformSocket(h) {}
		AwaitablePlatformSocket(PlatformSocket&& h) noexcept : PlatformSocket(std::forward<PlatformSocket>(h)) {
			Context_.RegisterSocket(Handle_);
		}
		AwaitablePlatformSocket(AwaitablePlatformSocket &&p) noexcept
		{
			Handle_.value = p.Handle_.value;
			p.Handle_.value = INVALID_SOCKET;
		}

		~AwaitablePlatformSocket() noexcept {
			Context_.DeregisterSocket(Handle_);
		}
		auto async_recv(std::byte* buffer, int buffer_size) {
			//auto m = std::min(buffer_size, 1024 * 1024);
			auto count = INTERNAL::Recv(Handle_, buffer, buffer_size);
			return INTERNAL::ReadAwaiter(buffer, buffer_size,  count,Handle_,  Context_);
		}
		auto async_send(std::byte* buffer, int buffer_size) {
			//auto m = std::min(buffer_size, 1024*1024);
			auto count = INTERNAL::Send(Handle_, buffer, buffer_size);
			return INTERNAL::WriteAwaiter(buffer, buffer_size, count, Handle_, Context_);
		}
		auto async_accept() {

			struct AcceptAwaiter : INTERNAL::Awaiter {
				const AddressFamily Family;
				char Buffer[(sizeof(SOCKADDR_STORAGE) + 16) * 2];
				SocketHandle ListenSocket;

				AcceptAwaiter(const AddressFamily &a, AwaitableContext &c, SocketHandle listensocket) noexcept : Family(a), ListenSocket(listensocket), INTERNAL::Awaiter(INVALID_SOCKET, IO_OPERATION::IoAccept, c) { }
				constexpr bool await_ready() const {
					return false;
				}
				auto await_resume() {
					if (StatusCode_ == StatusCode::SC_SUCCESS && ::setsockopt(SocketHandle_.value, SOL_SOCKET, SO_UPDATE_ACCEPT_CONTEXT, (char*)&ListenSocket.value, sizeof(ListenSocket.value)) == SOCKET_ERROR) {
						StatusCode_ = TranslateError();
					}
					AwaitablePlatformSocket s;
					s.Handle_ = SocketHandle_;
					return std::tuple(StatusCode_, AwaitablePlatformSocket(std::move(s)));
				}
				bool await_suspend(std::experimental::coroutine_handle<> h) {
					static INTERNAL::AcceptExCreator AcceptExCreator_;
					auto[status, socket] = INTERNAL::Create(Family);
					if (status == StatusCode::SC_SUCCESS) {
						status = Context.RegisterSocket(socket);
					}
					StatusCode_ = status;
					SocketHandle_ = socket;
					if (StatusCode_ != StatusCode::SC_SUCCESS) {
						Context.SetReadIO(SocketHandle_, nullptr);
						return false;
					}
					else {
						ResumeHandle = h;
						DWORD recvbytes(0);
						Context.SetReadIO(SocketHandle_, this);
						auto nRet = AcceptExCreator_.AcceptEx_(ListenSocket.value, SocketHandle_.value, (LPVOID)(Buffer), 0, sizeof(SOCKADDR_STORAGE) + 16, sizeof(SOCKADDR_STORAGE) + 16, &recvbytes, &Overlapped);
						if (nRet == TRUE) {
							StatusCode_ = StatusCode::SC_SUCCESS;
							Context.SetReadIO(SocketHandle_, nullptr);
							return false;
						}
						else if (auto err = WSAGetLastError(); !(nRet == FALSE && err == ERROR_IO_PENDING)) {
							StatusCode_ = TranslateError();
							Context.SetReadIO(SocketHandle_, nullptr);
							return false;
						}
					}
					return true;
				}
			};
			return AcceptAwaiter{ AddressFamily::IPANY , AwaitablePlatformSocket::Context_ , Handle_ };
		}
		friend auto async_connect(const SocketAddress &address);
	};

	bool INTERNAL::ReadAwaiter::await_suspend(std::experimental::coroutine_handle<> h) {
		ResumeHandle = h;
		WSABUF wsabuf;
		wsabuf.buf = reinterpret_cast<char*>(buffer);
		wsabuf.len = static_cast<decltype(wsabuf.len)>(remainingbytes);
		DWORD dwSendNumBytes(0), dwFlags(0);
		Context.SetReadIO(SocketHandle_, this);
		DWORD nRet = WSARecv(SocketHandle_.value, &wsabuf, 1, &dwSendNumBytes, &dwFlags, &(Overlapped), NULL);
		if (auto lasterr = WSAGetLastError(); nRet == SOCKET_ERROR && (WSA_IO_PENDING != lasterr)) {
			StatusCode_ = TranslateError();
			buffer_size = remainingbytes = 0;
			awaitready = true;
			Context.SetReadIO(SocketHandle_, nullptr);
			return false;
		}
		return true;
	}

	bool INTERNAL::WriteAwaiter::await_suspend(std::experimental::coroutine_handle<> h) {
		ResumeHandle = h;
		WSABUF wsabuf;
		wsabuf.buf = reinterpret_cast<char*>(buffer);
		wsabuf.len = static_cast<decltype(wsabuf.len)>(remainingbytes);
		DWORD dwSendNumBytes(0), dwFlags(0);
		Context.SetWriteIO(SocketHandle_, this);
		DWORD nRet = WSASend(SocketHandle_.value, &wsabuf, 1, &dwSendNumBytes, dwFlags, &(Overlapped), NULL);
		if (auto lasterr = WSAGetLastError(); nRet == SOCKET_ERROR && (WSA_IO_PENDING != lasterr)) {
			StatusCode_ = TranslateError();
			buffer_size = remainingbytes = 0;
			awaitready = true;
			Context.SetWriteIO(SocketHandle_, nullptr);
			return false;
		}
		return true;
	}

	inline auto async_connect(const SocketAddress &address) {

		struct ConnectAwaiter : INTERNAL::Awaiter {
			const SocketAddress& address;

			ConnectAwaiter(const SocketAddress &a, AwaitableContext &c) noexcept : address(a), INTERNAL::Awaiter(INVALID_SOCKET, IO_OPERATION::IoConnect, c) { }
			constexpr bool await_ready() const {
				return false;
			}
			auto await_resume() {
				if (StatusCode_ == StatusCode::SC_SUCCESS && ::setsockopt(SocketHandle_.value, SOL_SOCKET, SO_UPDATE_CONNECT_CONTEXT, 0, 0) == SOCKET_ERROR) {
					StatusCode_ = TranslateError();
				}
				AwaitablePlatformSocket s;
				s.Handle_ = SocketHandle_;
				return std::tuple(StatusCode_, AwaitablePlatformSocket(std::move(s)));
			}
			bool await_suspend(std::experimental::coroutine_handle<> h) {
				static INTERNAL::ConnectExCreator ConnectExCreator_;
				auto[status, socket] = INTERNAL::Create(address.getFamily());
				if (status == StatusCode::SC_SUCCESS) {
					if (INTERNAL::win32Bind(address.getFamily(), socket.value) == SOCKET_ERROR) {
						status = TranslateError();
					}
					else {
						status = Context.RegisterSocket(socket);
					}
				}
				StatusCode_ = status;
				SocketHandle_ = socket;
				if (StatusCode_ == StatusCode::SC_SUCCESS) {
					ResumeHandle = h;
					DWORD sendbytes(0);
					Context.SetReadIO(SocketHandle_, this);
					auto connectres = ConnectExCreator_.ConnectEx_(socket.value, address.getSocketAddr(), address.getSocketAddrLen(), 0, 0, &sendbytes, &Overlapped);
					if (connectres == TRUE) {
						StatusCode_ = StatusCode::SC_SUCCESS;
						Context.SetReadIO(SocketHandle_, nullptr);
						return false;
					}
					else if (auto err = WSAGetLastError(); !(connectres == FALSE && err == ERROR_IO_PENDING)) {
						StatusCode_ = TranslateError();
						Context.SetReadIO(SocketHandle_, nullptr);
						return false;
					}
				}
				return true;
			}
		};
		return ConnectAwaiter{ address , AwaitablePlatformSocket::Context_ };
	}

} // namespace SL::NET
