#include "net/socket_endpoint.hpp"
#include <gtest/gtest.h>

TEST(IPAddress, RejectsEmbeddedNuls)
{
    using namespace std::string_view_literals;
    EXPECT_FALSE(IPAddress::Parse("127.0.0.1\0junk"sv));
    EXPECT_FALSE(IPAddress::Parse("::1\0junk"sv));
    EXPECT_FALSE(IPAddress::Parse("127.0.0.1\0"sv));
    EXPECT_FALSE(IPAddress::Parse("\0::1"sv));
    EXPECT_TRUE(IPAddress::Parse("127.0.0.1"));
    EXPECT_TRUE(IPAddress::Parse("::1"));
    EXPECT_FALSE(IPAddress::Parse(""));
}

TEST(IPAddress, FromBytesValidatesVersionAndSize)
{
    const std::array<std::uint8_t, 4> bytes{ 127, 0, 0, 1 };
    const auto address = IPAddress::FromBytes(bytes, IPVersion::IPv4);
    ASSERT_TRUE(address);
    EXPECT_EQ(*address, IPAddress::Loopback(IPVersion::IPv4));
    EXPECT_FALSE(IPAddress::FromBytes(bytes, IPVersion::IPv6));
    EXPECT_FALSE(IPAddress::FromBytes({}, IPVersion::IPv4));
    EXPECT_FALSE(IPAddress::FromBytes(bytes, static_cast<IPVersion>(42)));
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
