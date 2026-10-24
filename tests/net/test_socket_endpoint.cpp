#include "net/socket_endpoint.hpp"
#include <gtest/gtest.h>
#include <array>
#include <cstring>
#include <limits>
#include <set>
#include <string_view>
#include <unordered_set>

using namespace net;

namespace
{
class ValidEndpoint : public testing::TestWithParam<const char*> {};

TEST_P(ValidEndpoint, ParsesAndRoundTrips)
{
    const auto endpoint = SocketEndpoint::Parse(GetParam());
    ASSERT_TRUE(endpoint);
    EXPECT_EQ(SocketEndpoint::Parse(endpoint->ToString()), endpoint);
    const auto [storage, length] = endpoint->ToNative();
    EXPECT_EQ(SocketEndpoint::FromNative(reinterpret_cast<const sockaddr*>(&storage), length), endpoint);
}

INSTANTIATE_TEST_SUITE_P(IPv4AndIPv6, ValidEndpoint, testing::Values(
    "0.0.0.0:0", "255.255.255.255:65535", "192.0.2.1:80", "127.0.0.1:00080",
    "[::]:0", "[::1]:65535", "[2001:db8::1]:443", "[::ffff:192.0.2.1]:80",
    "[fe80::1%0]:80", "[fe80::1%7]:8080", "[fe80::1%4294967295]:65535"));

class InvalidEndpoint : public testing::TestWithParam<const char*> {};

TEST_P(InvalidEndpoint, RejectsMalformedInput)
{
    EXPECT_FALSE(SocketEndpoint::Parse(GetParam()));
}

INSTANTIATE_TEST_SUITE_P(Malformed, InvalidEndpoint, testing::Values(
    "", "127.0.0.1", ":80", "127.0.0.1:", "localhost:80", "256.0.0.1:80",
    "127.0.0.1:-1", "127.0.0.1:+1", "127.0.0.1:65536", "127.0.0.1:999999999999999",
    "127.0.0.1:80junk", "127.0.0.1: 80", "127.0.0.1:80 ", "127.0.0.1:80:90",
    "::1:80", "[127.0.0.1]:80", "[::1", "[::1]", "[::1]80", "[]:80", "[::1]:",
    "[::1]:-1", "[::1]:+1", "[::1]:65536", "[::1]:80junk", "[::1]: 80", "[::1]:80 ",
    "[fe80::1%]:80", "[fe80::1%-1]:80", "[fe80::1%+1]:80", "[fe80::1%eth0]:80",
    "[fe80::1%4294967296]:80", "[fe80::1%7x]:80", "[fe80::1%7%8]:80"));
}

TEST(SocketEndpoint, DefaultAndExplicitConstruction)
{
    const SocketEndpoint empty;
    EXPECT_EQ(empty.GetAddress(), IPAddress::Any(IPVersion::IPv4));
    EXPECT_EQ(empty.GetVersion(), IPVersion::IPv4);
    EXPECT_EQ(empty.GetPort(), 0);
    EXPECT_EQ(empty.GetScopeId(), 0u);
    EXPECT_EQ(empty.ToString(), "0.0.0.0:0");
    const SocketEndpoint v4{ IPAddress::Loopback(IPVersion::IPv4), 65535, 99 };
    EXPECT_EQ(v4.GetPort(), 65535);
    EXPECT_EQ(v4.GetScopeId(), 0u);
    EXPECT_EQ(v4.ToString(), "127.0.0.1:65535");
    const SocketEndpoint v6{ IPAddress::Loopback(IPVersion::IPv6), 443 };
    EXPECT_EQ(v6.GetVersion(), IPVersion::IPv6);
    EXPECT_EQ(v6.ToString(), "[::1]:443");
}

TEST(SocketEndpoint, RejectsEmbeddedNulsAndSupportsBoundedStringViews)
{
    using namespace std::string_view_literals;
    for (auto text : { "127.0.0.1\0:80"sv, "127.0.0.1:80\0junk"sv,
        "[::1\0]:80"sv, "[::1]:80\0"sv, "[fe80::1%7\0]:80"sv })
        EXPECT_FALSE(SocketEndpoint::Parse(text));
    const auto bounded = "[::1]:80junk"sv.substr(0, 8);
    EXPECT_EQ(SocketEndpoint::Parse(bounded), SocketEndpoint::Parse("[::1]:80"));
}

TEST(SocketEndpoint, NativeIPv4UsesNetworkByteOrder)
{
    const auto endpoint = SocketEndpoint::Parse("192.0.2.42:4660");
    ASSERT_TRUE(endpoint);
    const auto [storage, length] = endpoint->ToNative();
    EXPECT_EQ(length, static_cast<socklen_t>(sizeof(sockaddr_in)));
    sockaddr_in native{};
    std::memcpy(&native, &storage, sizeof(native));
    EXPECT_EQ(native.sin_family, AF_INET);
    EXPECT_EQ(ntohs(native.sin_port), 4660);
    const std::array<std::uint8_t, 4> expected{ 192, 0, 2, 42 };
    EXPECT_EQ(std::memcmp(&native.sin_addr, expected.data(), expected.size()), 0);
    const auto* portBytes = reinterpret_cast<const std::uint8_t*>(&native.sin_port);
    EXPECT_EQ(portBytes[0], 0x12);
    EXPECT_EQ(portBytes[1], 0x34);
}

TEST(SocketEndpoint, NativeIPv6PreservesScopeAndNetworkByteOrder)
{
    const auto endpoint = SocketEndpoint::Parse("[2001:db8::1%4294967295]:4660");
    ASSERT_TRUE(endpoint);
    const auto [storage, length] = endpoint->ToNative();
    EXPECT_EQ(length, static_cast<socklen_t>(sizeof(sockaddr_in6)));
    sockaddr_in6 native{};
    std::memcpy(&native, &storage, sizeof(native));
    EXPECT_EQ(native.sin6_family, AF_INET6);
    EXPECT_EQ(ntohs(native.sin6_port), 4660);
    EXPECT_EQ(native.sin6_scope_id, std::numeric_limits<std::uint32_t>::max());
    EXPECT_EQ(native.sin6_flowinfo, 0u);
    std::array<std::uint8_t, 16> expected{ 0x20, 0x01, 0x0d, 0xb8 };
    expected[15] = 1;
    EXPECT_EQ(std::memcmp(&native.sin6_addr, expected.data(), expected.size()), 0);
    const auto* portBytes = reinterpret_cast<const std::uint8_t*>(&native.sin6_port);
    EXPECT_EQ(portBytes[0], 0x12);
    EXPECT_EQ(portBytes[1], 0x34);
}

TEST(SocketEndpoint, ImportsIndependentlyPopulatedNativeAddresses)
{
    sockaddr_in v4{};
    v4.sin_family = AF_INET;
    v4.sin_port = htons(12345);
    ASSERT_EQ(::inet_pton(AF_INET, "192.0.2.42", &v4.sin_addr), 1);
    EXPECT_EQ(SocketEndpoint::FromNative(reinterpret_cast<const sockaddr*>(&v4), sizeof(v4)),
        SocketEndpoint::Parse("192.0.2.42:12345"));

    sockaddr_in6 v6{};
    v6.sin6_family = AF_INET6;
    v6.sin6_port = htons(65535);
    v6.sin6_scope_id = 7;
    v6.sin6_flowinfo = 123;
    ASSERT_EQ(::inet_pton(AF_INET6, "fe80::42", &v6.sin6_addr), 1);
    EXPECT_EQ(SocketEndpoint::FromNative(reinterpret_cast<const sockaddr*>(&v6), sizeof(v6)),
        SocketEndpoint::Parse("[fe80::42%7]:65535"));
}

TEST(SocketEndpoint, RejectsUnsupportedNativeFamilyAndAcceptsLargerStorage)
{
    sockaddr unsupported{};
    unsupported.sa_family = AF_UNSPEC;
    EXPECT_FALSE(SocketEndpoint::FromNative(&unsupported, sizeof(unsupported)));
    EXPECT_FALSE(SocketEndpoint::FromNative(nullptr, sizeof(sockaddr_storage)));
    for (auto version : { IPVersion::IPv4, IPVersion::IPv6 })
    {
        const SocketEndpoint original{ IPAddress::Any(version), 0 };
        const auto [storage, length] = original.ToNative();
        EXPECT_EQ(SocketEndpoint::FromNative(reinterpret_cast<const sockaddr*>(&storage), sizeof(storage)), original);
        if constexpr (std::numeric_limits<socklen_t>::is_signed)
        {
            EXPECT_FALSE(SocketEndpoint::FromNative(reinterpret_cast<const sockaddr*>(&storage), -1));
        }
    }
}

TEST(SocketEndpoint, ComparesAddressThenPortThenScopeAndSupportsContainerKeys)
{
    const SocketEndpoint first{ IPAddress::Loopback(IPVersion::IPv4), 80 };
    const SocketEndpoint nextPort{ first.GetAddress(), 81 };
    const SocketEndpoint nextAddress{ IPAddress::Any(IPVersion::IPv6), 0 };
    const SocketEndpoint scope7{ IPAddress::Loopback(IPVersion::IPv6), 80, 7 };
    const SocketEndpoint scope8{ scope7.GetAddress(), 80, 8 };
    EXPECT_LT(first, nextPort);
    EXPECT_LT(nextPort, nextAddress);
    EXPECT_LT(scope7, scope8);
    EXPECT_GT(scope8, scope7);
    EXPECT_NE(scope7, scope8);
    EXPECT_NE(first, nextPort);
    EXPECT_NE(first, nextAddress);
    EXPECT_EQ(first <=> first, std::strong_ordering::equal);
    const auto copy = first;
    EXPECT_EQ(first.Hash(), copy.Hash());
    EXPECT_EQ(std::hash<SocketEndpoint>{}(first), first.Hash());
    const std::set<SocketEndpoint> ordered{ first, copy, nextPort, scope7, scope8 };
    const std::unordered_set<SocketEndpoint> hashed{ first, copy, nextPort, scope7, scope8 };
    EXPECT_EQ(ordered.size(), 4u);
    EXPECT_EQ(hashed.size(), 4u);
    EXPECT_TRUE(hashed.contains(copy));
}

TEST(SocketEndpoint, NativeRoundTrip)
{
    for (auto version : { IPVersion::IPv4, IPVersion::IPv6 })
    {
        const SocketEndpoint original{ IPAddress::Loopback(version), 12345 };
        const auto [storage, length] = original.ToNative();
        const auto restored = SocketEndpoint::FromNative(
            reinterpret_cast<const sockaddr*>(&storage), length);
        ASSERT_TRUE(restored);
        EXPECT_EQ(*restored, original);
    }
}

TEST(SocketEndpoint, RejectsTruncatedNativeAddresses)
{
    EXPECT_FALSE(SocketEndpoint::FromNative(nullptr, 0));

    // A family read from this buffer would overrun it under AddressSanitizer.
    alignas(sockaddr) const std::array<std::uint8_t, 1> truncated{};
    EXPECT_FALSE(SocketEndpoint::FromNative(
        reinterpret_cast<const sockaddr*>(truncated.data()),
        static_cast<socklen_t>(truncated.size())));

    for (auto version : { IPVersion::IPv4, IPVersion::IPv6 })
    {
        const SocketEndpoint endpoint{ IPAddress::Loopback(version), 12345 };
        const auto [storage, length] = endpoint.ToNative();
        for (socklen_t len = 0; len < length; ++len)
        {
            EXPECT_FALSE(SocketEndpoint::FromNative(
                reinterpret_cast<const sockaddr*>(&storage), len));
        }
    }
}

TEST(SocketEndpoint, PreservesIPv6Scope)
{
    const auto address = IPAddress::Parse("fe80::1");
    ASSERT_TRUE(address);
    const SocketEndpoint original{ *address, 8080, 7 };
    const auto [storage, length] = original.ToNative();
    const auto restored = SocketEndpoint::FromNative(
        reinterpret_cast<const sockaddr*>(&storage), length);
    ASSERT_TRUE(restored);
    EXPECT_EQ(restored->GetScopeId(), 7u);
    EXPECT_EQ(*restored, original);
    EXPECT_EQ(restored->ToString(), "[fe80::1%7]:8080");
    EXPECT_NE(original, (SocketEndpoint{ *address, 8080, 8 }));
    EXPECT_EQ((SocketEndpoint{ IPAddress::Loopback(IPVersion::IPv4), 80, 7 }).GetScopeId(), 0u);
}

