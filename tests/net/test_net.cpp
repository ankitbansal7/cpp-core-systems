#include "net/socket_endpoint.hpp"
#include <gtest/gtest.h>

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
