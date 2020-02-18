#ifndef SL_NETWORK_IO_THREAD_SERVICE
#define SL_NETWORK_IO_THREAD_SERVICE

#include "io_service.h"

namespace SL::Network {
	class io_thread_service {
	public:

		io_thread_service(std::uint32_t num_threads = std::thread::hardware_concurrency()) : ioservice(num_threads) {
			runningcount = 0;
			threads.reserve(num_threads);
			for (std::uint32_t i = 0; i < num_threads; i++) {
				threads.emplace_back(std::thread([&]() {
					runningcount += 1;
					ioservice.run();
					runningcount -= 1;
					}));
			}
		}

		~io_thread_service() {
			ioservice.stop();
			while (runningcount != 0) {
				std::this_thread::sleep_for(std::chrono::milliseconds(20));
			}
			for (auto& t : threads) {
				t.join();
			}
		}

		io_thread_service(io_thread_service&& other) = delete;
		io_thread_service(const io_thread_service& other) = delete;
		io_thread_service& operator=(io_thread_service&& other) = delete;
		io_thread_service& operator=(const io_thread_service& other) = delete;

		io_service& getio_service() { return ioservice; }

	private:
		io_service ioservice;
		std::vector<std::thread> threads;
		std::atomic<size_t> runningcount;
	};
}

#endif
