#ifndef SL_NETWORK_IP6_ENDPOINTADDRESS
#define SL_NETWORK_IP6_ENDPOINTADDRESS

#include "ipv6_address.h"

#include <optional>
#include <string>
#include <string_view>

namespace SL::Network {

	class ipv6_endpoint
	{
	public:

		// Construct to [::]:0
		constexpr ipv6_endpoint() noexcept : m_address(), m_port(0) {}
		constexpr explicit ipv6_endpoint(ipv6_address address, std::uint16_t port) noexcept : m_address(address), m_port(port) {}

		constexpr const ipv6_address& address() const noexcept { return m_address; }
		constexpr std::uint16_t port() const noexcept { return m_port; }

		std::string to_string() const;
		static std::optional<ipv6_endpoint> from_string(std::string_view string) noexcept;

		constexpr std::weak_ordering operator<=>(ipv6_endpoint const& rhs) const = default; 
	private:

		ipv6_address m_address;
		std::uint16_t m_port;

	}; 
}


#endif
