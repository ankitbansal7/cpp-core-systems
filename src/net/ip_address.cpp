#include "net/platform.hpp"
#include "net/ip_address.hpp"

std::optional<IPAddress> IPAddress::Parse(std::string_view address)
{
    IPAddress result;
    in_addr ipv4{};

    if (::inet_pton(AF_INET, address.data(), &ipv4) == 1)
    {
        result.m_version = IPVersion::IPv4;
        std::memcpy(result.m_bytes.data(), &ipv4, sizeof(ipv4));
        return result;
    }

    in6_addr ipv6{};

    if (::inet_pton(AF_INET6, address.data(), &ipv6) == 1)
    {
        result.m_version = IPVersion::IPv6;
        std::memcpy(result.m_bytes.data(), &ipv6, sizeof(ipv6));
        return result;
    }

    return std::nullopt;
}

IPAddress IPAddress::Loopback(IPVersion version) noexcept
{
    IPAddress address;
    address.m_version = version;

    if (version == IPVersion::IPv4)
    {
        // 127.0.0.1
        address.m_bytes[0] = 127;
        address.m_bytes[3] = 1;
    }
    else
    {
        // ::1
        address.m_bytes[15] = 1;
    }

    return address;
}

IPAddress IPAddress::Any(IPVersion version) noexcept
{
    IPAddress address;
    address.m_version = version;
    return address; // 0.0.0.0 or ::
}

IPVersion IPAddress::GetVersion() const noexcept
{
    return m_version;
}

std::string IPAddress::ToString() const
{
    char buffer[INET6_ADDRSTRLEN]{};

    if (m_version == IPVersion::IPv4)
    {
        if (::inet_ntop(AF_INET, m_bytes.data(), buffer, sizeof(buffer)) == nullptr)
        {
            return {};
        }
    }
    else
    {
        if (::inet_ntop(AF_INET6, m_bytes.data(), buffer, sizeof(buffer)) == nullptr)
        {
            return {};
        }
    }

    return std::string{ buffer };
}

std::size_t IPAddress::Hash() const noexcept
{
    constexpr std::size_t offset =
        (sizeof(std::size_t) == 8)
        ? 14695981039346656037ull
        : 2166136261u;

    constexpr std::size_t prime =
        (sizeof(std::size_t) == 8)
        ? 1099511628211ull
        : 16777619u;

    std::size_t hash = offset;
    const std::size_t byteCount = (m_version == IPVersion::IPv4 ? 4 : 16);

    hash ^= static_cast<std::size_t>(m_version);
    hash *= prime;

    for (std::size_t i = 0; i < byteCount; ++i)
    {
        hash ^= m_bytes[i];
        hash *= prime;
    }

    return hash;
}
