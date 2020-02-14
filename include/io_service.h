#ifndef SL_NETWORK_IO_SERVICE
#define SL_NETWORK_IO_SERVICE

#include <cstdint> 
#include "utils.h"

namespace SL::Network {
	class io_service {
	public:

		io_service(std::uint32_t concurrencyHint);
		~io_service();

		io_service(io_service&& other) = delete;
		io_service(const io_service& other) = delete;
		io_service& operator=(io_service&& other) = delete;
		io_service& operator=(const io_service& other) = delete;
		void run();
		void stop();
#if _WIN32
		void* getHandle() const { return IOCPHandle.handle; }

	private:
		win32::safe_handle IOCPHandle;
#else
		int EventWakeFd;
		int IOCPHandle;
#endif
		bool KeepGoing;
	};
}

#endif
