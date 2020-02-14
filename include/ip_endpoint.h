#ifndef SL_NETWORK_IP_ENDPOINTADDRESS
#define SL_NETWORK_IP_ENDPOINTADDRESS

#include "ip_address.h"
#include "ipv4_endpoint.h"
#include "ipv6_endpoint.h"

#include <cassert>
#include <optional>
#include <string>
#include <compare>

namespace SL::Network {

	class ip_endpoint
	{
	public:

		// Constructs to IPv4 end-point 0.0.0.0:0
		constexpr ip_endpoint() noexcept : m_family(family::ipv4), m_ipv4() {}
		constexpr ip_endpoint(ipv4_endpoint endpoint) noexcept : m_family(family::ipv4), m_ipv4(endpoint) {}
		constexpr ip_endpoint(ipv6_endpoint endpoint) noexcept : m_family(family::ipv6), m_ipv6(endpoint) {}

		constexpr bool is_ipv4() const noexcept { return m_family == family::ipv4; }
		constexpr bool is_ipv6() const noexcept { return m_family == family::ipv6; }

		constexpr const ipv4_endpoint& to_ipv4() const { assert(is_ipv4()); return m_ipv4; }
		constexpr const ipv6_endpoint& to_ipv6() const { assert(is_ipv6()); return m_ipv6; }

		constexpr ip_address address() const noexcept { return is_ipv4() ? ip_address(m_ipv4.address()) : ip_address(m_ipv6.address()); }
		constexpr std::uint16_t port() const noexcept { return is_ipv4() ? m_ipv4.port() : m_ipv6.port(); }

		std::string to_string() const { return is_ipv4() ? m_ipv4.to_string() : m_ipv6.to_string(); }
		static std::optional<ip_endpoint> from_string(std::string_view string) noexcept;

		constexpr std::weak_ordering operator<=>(ip_endpoint const& rhs) const { return (rhs < *this) <=> false; } 
	private:

		enum class family
		{
			ipv4,
			ipv6
		};

		family m_family;

		union
		{
			ipv4_endpoint m_ipv4;
			ipv6_endpoint m_ipv6;
		};
	};
	std::optional<SL::Network::ip_endpoint> SL::Network::ip_endpoint::from_string(std::string_view string) noexcept
	{
		if (auto ipv4 = ipv4_endpoint::from_string(string); ipv4)
		{
			return *ipv4;
		}

		if (auto ipv6 = ipv6_endpoint::from_string(string); ipv6)
		{
			return *ipv6;
		}

		return std::nullopt;
	}

}

#endif
