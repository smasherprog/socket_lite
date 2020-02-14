#include "ip_endpoint.h"

std::string SL::Network::ip_endpoint::to_string() const
{
	return is_ipv4() ? m_ipv4.to_string() : m_ipv6.to_string();
}

std::optional<SL::Network::ip_endpoint>
SL::Network::ip_endpoint::from_string(std::string_view string) noexcept
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
