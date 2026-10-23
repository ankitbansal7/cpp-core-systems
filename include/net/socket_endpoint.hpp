#pragma once

#include "net/platform.hpp"
#include "net/ip_address.hpp"

#include <cstdint>
#include <string>
#include <string_view>
#include <optional>
#include <cstring>
#include <array>
#include <utility>
#include <compare>
#include <charconv>

class SocketEndpoint final
{
public:
    SocketEndpoint() noexcept = default;

    SocketEndpoint(const IPAddress& address, std::uint16_t port, std::uint32_t scopeId = 0) noexcept
        : m_address(address)
        , m_port(port)
        , m_scopeId(address.GetVersion() == IPVersion::IPv6 ? scopeId : 0)
    {
    }

    [[nodiscard]]
    const IPAddress& GetAddress() const noexcept
    {
        return m_address;
    }

    [[nodiscard]]
    std::uint16_t GetPort() const noexcept
    {
        return m_port;
    }

    [[nodiscard]]
    IPVersion GetVersion() const noexcept
    {
        return m_address.GetVersion();
    }

    [[nodiscard]]
    std::uint32_t GetScopeId() const noexcept
    {
        return m_scopeId;
    }

    [[nodiscard]]
    std::string ToString() const
    {
        if (m_address.GetVersion() == IPVersion::IPv6)
        {
            const auto scope = m_scopeId == 0 ? std::string{} : "%" + std::to_string(m_scopeId);
            return "[" + m_address.ToString() + scope + "]:" + std::to_string(m_port);
        }

        return m_address.ToString() + ":" + std::to_string(m_port);
    }

    [[nodiscard]]
    static std::optional<SocketEndpoint> Parse(std::string_view str) noexcept
    {
        if (str.empty())
        {
            return std::nullopt;
        }

        if (str.front() == '[')
        {
            const auto closeBracket = str.find(']');

            if (closeBracket == std::string_view::npos || closeBracket + 1 >= str.size() || str[closeBracket + 1] != ':')
            {
                return std::nullopt;
            }

            std::string_view ipPart = str.substr(1, closeBracket - 1);
            std::string_view portPart = str.substr(closeBracket + 2);

            std::uint32_t scopeId = 0;
            const auto percent = ipPart.find('%');

            if (percent != std::string_view::npos)
            {
                std::string_view scopePart = ipPart.substr(percent + 1);
                ipPart = ipPart.substr(0, percent);

                auto [ptr, ec] = std::from_chars(scopePart.data(), scopePart.data() + scopePart.size(), scopeId);

                if (ec != std::errc{} || ptr != scopePart.data() + scopePart.size())
                {
                    return std::nullopt;
                }
            }

            auto ip = IPAddress::Parse(ipPart);

            if (!ip || ip->GetVersion() != IPVersion::IPv6)
            {
                return std::nullopt;
            }

            std::uint16_t port = 0;
            auto [ptr, ec] = std::from_chars(portPart.data(), portPart.data() + portPart.size(), port);

            if (ec != std::errc{} || ptr != portPart.data() + portPart.size())
            {
                return std::nullopt;
            }

            return SocketEndpoint{ *ip, port, scopeId };
        }

        const auto colon = str.rfind(':');

        if (colon == std::string_view::npos || colon == 0 || colon + 1 >= str.size())
        {
            return std::nullopt;
        }

        std::string_view ipPart = str.substr(0, colon);
        std::string_view portPart = str.substr(colon + 1);

        auto ip = IPAddress::Parse(ipPart);

        if (!ip || ip->GetVersion() != IPVersion::IPv4)
        {
            return std::nullopt;
        }

        std::uint16_t port = 0;
        auto [ptr, ec] = std::from_chars(portPart.data(), portPart.data() + portPart.size(), port);

        if (ec != std::errc{} || ptr != portPart.data() + portPart.size())
        {
            return std::nullopt;
        }

        return SocketEndpoint{ *ip, port };
    }

    // --- OS Interop Helpers ---

    [[nodiscard]]
    std::pair<sockaddr_storage, socklen_t> ToNative() const noexcept
    {
        sockaddr_storage storage{};

        if (m_address.GetVersion() == IPVersion::IPv4)
        {
            sockaddr_in addr4{};
            addr4.sin_family = AF_INET;
            addr4.sin_port = htons(m_port);
            std::memcpy(&addr4.sin_addr, m_address.Bytes(), IPAddress::IPV4_BYTE_COUNT);
            std::memcpy(&storage, &addr4, sizeof(addr4));
            return { storage, static_cast<socklen_t>(sizeof(sockaddr_in)) };
        }
        else
        {
            sockaddr_in6 addr6{};
            addr6.sin6_family = AF_INET6;
            addr6.sin6_port = htons(m_port);
            addr6.sin6_scope_id = m_scopeId;
            std::memcpy(&addr6.sin6_addr, m_address.Bytes(), IPAddress::IPV6_BYTE_COUNT);
            std::memcpy(&storage, &addr6, sizeof(addr6));
            return { storage, static_cast<socklen_t>(sizeof(sockaddr_in6)) };
        }
    }

    [[nodiscard]]
    static std::optional<SocketEndpoint> FromNative(const sockaddr* addr, socklen_t len)
    {
        if (addr == nullptr)
        {
            return std::nullopt;
        }

        if ((addr->sa_family == AF_INET) && (len >= static_cast<socklen_t>(sizeof(sockaddr_in))))
        {
            sockaddr_in addr4{};
            std::memcpy(&addr4, addr, sizeof(addr4));

            std::array<std::uint8_t, IPAddress::IPV4_BYTE_COUNT> bytes{};
            std::memcpy(bytes.data(), &addr4.sin_addr, bytes.size());

            if (auto ip = IPAddress::FromBytes(bytes, IPVersion::IPv4))
            {
                return SocketEndpoint{ *ip, ntohs(addr4.sin_port) };
            }
        }
        else if (addr->sa_family == AF_INET6 && len >= static_cast<socklen_t>(sizeof(sockaddr_in6)))
        {
            sockaddr_in6 addr6{};
            std::memcpy(&addr6, addr, sizeof(addr6));

            std::array<std::uint8_t, IPAddress::IPV6_BYTE_COUNT> bytes{};
            std::memcpy(bytes.data(), &addr6.sin6_addr, bytes.size());

            if (auto ip = IPAddress::FromBytes(bytes, IPVersion::IPv6))
            {
                return SocketEndpoint{ *ip, ntohs(addr6.sin6_port), addr6.sin6_scope_id };
            }
        }

        return std::nullopt;
    }

    // --- Relational Operators ---

    [[nodiscard]]
    friend bool operator==(const SocketEndpoint& lhs, const SocketEndpoint& rhs) noexcept
    {
        return (lhs.m_address == rhs.m_address) && (lhs.m_port == rhs.m_port) && (lhs.m_scopeId == rhs.m_scopeId);
    }

    [[nodiscard]]
    friend auto operator<=>(const SocketEndpoint& lhs, const SocketEndpoint& rhs) noexcept
    {
        if (auto cmp = lhs.m_address <=> rhs.m_address; cmp != 0)
        {
            return cmp;
        }
        if (auto cmp = lhs.m_port <=> rhs.m_port; cmp != 0)
        {
            return cmp;
        }
        return lhs.m_scopeId <=> rhs.m_scopeId;
    }

    // --- Hashing ---

    [[nodiscard]]
    std::size_t Hash() const noexcept
    {
        constexpr std::size_t prime = (sizeof(std::size_t) == 8) ? 1099511628211ull : 16777619u;
        std::size_t hash = m_address.Hash();

        hash ^= static_cast<std::size_t>(m_port);
        hash *= prime;

        hash ^= static_cast<std::size_t>(m_scopeId);
        hash *= prime;

        return hash;
    }

private:
    IPAddress m_address{ IPAddress::Any(IPVersion::IPv4) };
    std::uint16_t m_port{ 0 };
    std::uint32_t m_scopeId{ 0 };
};

namespace std
{
    template <>
    struct hash<SocketEndpoint>
    {
        std::size_t operator()(const SocketEndpoint& endpoint) const noexcept
        {
            return endpoint.Hash();
        }
    };
}
