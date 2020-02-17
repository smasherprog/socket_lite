#ifndef SL_NETWORK_IO_SERVICE
#define SL_NETWORK_IO_SERVICE

#include <cstdint> 
#include "utils.h"

namespace SL::Network {
	class io_service {
	public:

		io_service(std::uint32_t concurrencyHint = std::thread::hardware_concurrency());
		~io_service();

		io_service(io_service&& other) = delete;
		io_service(const io_service& other) = delete;
		io_service& operator=(io_service&& other) = delete;
		io_service& operator=(const io_service& other) = delete;
		void run();
		void stop();
		void* getHandle() const { return IOCPHandle.handle(); }

	private:
		safe_handle IOCPHandle;
#ifndef _WIN32 
		int EventWakeFd;
#endif
		bool KeepGoing;
	};
}

#endif
