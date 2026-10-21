#pragma once

#include "net/platform.hpp"

#include <string>
#include <string_view>
#include <optional>
#include <array>
#include <cstdint>
#include <cstddef>
#include <algorithm>
#include <cstring>
#include <functional>
#include <span>

enum class IPVersion
{
    IPv4,
    IPv6
};

/// Represents an IPv4 or IPv6 address.
///
/// IPAddress supports equality comparison and can be used directly
/// as a key in standard unordered containers such as
/// std::unordered_map and std::unordered_set.
class IPAddress final
{
private:
    IPAddress() = default;

public:
    [[nodiscard]]
    static std::optional<IPAddress> FromBytes(std::span<const std::uint8_t> bytes,
        IPVersion version) noexcept
    {
        if (version != IPVersion::IPv4 && version != IPVersion::IPv6)
        {
            return std::nullopt;
        }

        const std::size_t expected = (version == IPVersion::IPv4) ? IPV4_BYTE_COUNT : IPV6_BYTE_COUNT;
        if (bytes.size() != expected)
        {
            return std::nullopt;
        }

        IPAddress address;
        address.m_version = version;
        std::copy(bytes.begin(), bytes.end(), address.m_bytes.begin());
        return address;
    }

    [[nodiscard]]
    static std::optional<IPAddress> Parse(std::string_view address)
    {
        std::string addressStr(address);
        IPAddress result;
        in_addr ipv4{};

        if (::inet_pton(AF_INET, addressStr.c_str(), &ipv4) == 1)
        {
            result.m_version = IPVersion::IPv4;
            std::memcpy(result.m_bytes.data(), &ipv4, sizeof(ipv4));
            return result;
        }

        in6_addr ipv6{};

        if (::inet_pton(AF_INET6, addressStr.c_str(), &ipv6) == 1)
        {
            result.m_version = IPVersion::IPv6;
            std::memcpy(result.m_bytes.data(), &ipv6, sizeof(ipv6));
            return result;
        }

        return std::nullopt;
    }

    [[nodiscard]]
    static IPAddress Loopback(IPVersion version) noexcept
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

    [[nodiscard]]
    static IPAddress Any(IPVersion version) noexcept
    {
        IPAddress address;
        address.m_version = version;
        return address;
    }

    [[nodiscard]]
    IPVersion GetVersion() const noexcept
    {
        return m_version;
    }

    // Raw pointer overload for C-style APIs (inet_ntop, sockaddr population)
    [[nodiscard]]
    const std::uint8_t* Bytes() const noexcept
    {
        return m_bytes.data();
    }

    [[nodiscard]]
    std::size_t ByteCount() const noexcept
    {
        return (m_version == IPVersion::IPv4) ? IPV4_BYTE_COUNT : IPV6_BYTE_COUNT;
    }

    [[nodiscard]]
    std::string ToString() const
    {
        char buffer[INET6_ADDRSTRLEN]{};
        int family = (m_version == IPVersion::IPv4) ? AF_INET : AF_INET6;

        if (::inet_ntop(family, m_bytes.data(), buffer, sizeof(buffer)) == nullptr)
        {
            return std::string{};
        }

        return std::string{ buffer };
    }

    [[nodiscard]]
    bool operator==(const IPAddress& other) const noexcept
    {
        if (m_version != other.m_version)
        {
            return false;
        }

        return std::equal(m_bytes.begin(), m_bytes.begin() + ByteCount(), other.m_bytes.begin());
    }

    [[nodiscard]]
    bool operator!=(const IPAddress& other) const noexcept
    {
        return !(*this == other);
    }

    [[nodiscard]]
    std::size_t Hash() const noexcept
    {
        constexpr std::size_t offset = (sizeof(std::size_t) == 8) ? 14695981039346656037ull : 2166136261u;
        constexpr std::size_t prime = (sizeof(std::size_t) == 8) ? 1099511628211ull : 16777619u;

        std::size_t hash = offset;
        const std::size_t byteCount = ByteCount();

        hash ^= static_cast<std::size_t>(m_version);
        hash *= prime;

        for (std::size_t i = 0; i < byteCount; ++i)
        {
            hash ^= m_bytes[i];
            hash *= prime;
        }

        return hash;
    }

public:
    static constexpr std::size_t IPV4_BYTE_COUNT{ 4 };
    static constexpr std::size_t IPV6_BYTE_COUNT{ 16 };

private:
    IPVersion m_version{ IPVersion::IPv4 };
    std::array<std::uint8_t, IPV6_BYTE_COUNT> m_bytes{};
};

namespace std
{

    template <>
    struct hash<IPAddress>
    {
        std::size_t operator()(const IPAddress& address) const noexcept
        {
            return address.Hash();
        }
    };

}

