#pragma once

#include "utils.h"
#include "io_service.h"
#include "socket.h"
#include <thread>
#include <mutex> 

namespace SL::Network {

	template <class SOCKETHANDLERTYPE> class async_acceptor {
		socket AcceptorSocket;
		io_service& Context_;
		SOCKETHANDLERTYPE Handler;
		bool Keepgoing = true;
		std::vector<std::thread> threads;
		std::atomic<size_t> runningcount;
		std::mutex mutex;

	public:
		async_acceptor(socket& s, const SOCKETHANDLERTYPE& callback, std::uint32_t num_threads = 4) :
			Context_(s.get_ioservice()),
			AcceptorSocket(std::move(s)),
			Handler(callback) {
			runningcount = 0;
			threads.reserve(num_threads);
			for (std::uint32_t i = 0; i < num_threads; i++) {
				threads.emplace_back(std::thread([&]() {
					runningcount += 1;

					while (Keepgoing) {
#if _WIN32
						auto handle = INVALID_SOCKET;
						{
							std::lock_guard<std::mutex> lk(mutex);
							if (!Keepgoing) break;
							handle = ::accept(AcceptorSocket.native_handle(), NULL, NULL);
						}
						if (handle != INVALID_SOCKET) {
							socket s(handle, Context_);
							if (CreateIoCompletionPort((HANDLE)handle, Context_.getHandle(), handle, NULL) != NULL) {
								if (SetFileCompletionNotificationModes((HANDLE)handle, FILE_SKIP_COMPLETION_PORT_ON_SUCCESS) == TRUE) {
									Handler(std::move(s));
								}
							}
						}
#else
						{
							SocketHandle handle = ::accept4(AcceptorSocket.Handle().value, NULL, NULL, SOCK_NONBLOCK);
							if (handle.value != INVALID_SOCKET) {
								AsyncPlatformSocket s(Context_, handle);
								epoll_event ev = { 0 };
								ev.events = EPOLLONESHOT;
								if (epoll_ctl(ContextImpl_.getIOHandle(), EPOLL_CTL_ADD, handle.value, &ev) == 0) {
									Handler(std::move(s));
								}
							}
						}
#endif
					}
					runningcount -= 1;
					}));
			}
		}

		~async_acceptor() {
			stop();
		}
		void stop() {
			Keepgoing = false;
#if _WIN32
			AcceptorSocket.close();
#else
			AcceptorSocket.shutdown(ShutDownOptions::SHUTDOWN_READ);
#endif 
			while (runningcount != 0) {
				std::this_thread::sleep_for(std::chrono::milliseconds(20));
			}
			for (auto& t : threads) {
				t.join();
			}
			threads.clear();
		}

		async_acceptor(async_acceptor&& other) = delete;
		async_acceptor(const async_acceptor& other) = delete;
		async_acceptor& operator=(async_acceptor&& other) = delete;
		async_acceptor& operator=(const async_acceptor& other) = delete;
	};
} // namespace SL::NET
