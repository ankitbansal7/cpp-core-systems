#include "net/ip_address.hpp"
#include <gtest/gtest.h>
#include <array>
#include <set>
#include <string>
#include <string_view>
#include <unordered_set>

using namespace net;

namespace
{
class ValidIPAddress : public testing::TestWithParam<const char*> {};

TEST_P(ValidIPAddress, ParsesAndRoundTrips)
{
    const auto address = IPAddress::Parse(GetParam());
    ASSERT_TRUE(address);
    EXPECT_EQ(IPAddress::Parse(address->ToString()), address);
    EXPECT_EQ(IPAddress::FromBytes(address->SpanBytes(), address->GetVersion()), address);
    EXPECT_EQ(address->SpanBytes().data(), address->Bytes());
    EXPECT_EQ(address->SpanBytes().size(), address->ByteCount());
    EXPECT_EQ(address->ByteCount(), address->GetVersion() == IPVersion::IPv4 ? 4u : 16u);
}

INSTANTIATE_TEST_SUITE_P(IPv4AndIPv6, ValidIPAddress, testing::Values(
    "0.0.0.0", "127.0.0.1", "192.0.2.255", "255.255.255.255",
    "::", "::1", "2001:db8::1234", "2001:DB8:1:2:3:4:5:6",
    "ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff", "::ffff:192.0.2.1",
    "ffff:ffff:ffff:ffff:ffff:ffff:255.255.255.255"));

class InvalidIPAddress : public testing::TestWithParam<const char*> {};

TEST_P(InvalidIPAddress, RejectsMalformedInput)
{
    EXPECT_FALSE(IPAddress::Parse(GetParam()));
}

INSTANTIATE_TEST_SUITE_P(Malformed, InvalidIPAddress, testing::Values(
    "", "localhost", "127.1", "256.0.0.1", "1.2.3", "1.2.3.4.5",
    "-1.0.0.1", "0x7f.0.0.1", " 127.0.0.1", "127.0.0.1 ",
    "127.0.0.1:80", "[::1]", "fe80::1%7", ":", ":::1", "1::2::3",
    "1:2:3:4:5:6:7", "1:2:3:4:5:6:7:8:9", "12345::", "gggg::"));
}

TEST(IPAddress, RejectsEmbeddedNulsAndOversizedInput)
{
    using namespace std::string_view_literals;
    for (auto text : { "127.0.0.1\0junk"sv, "::1\0junk"sv, "127.0.0.1\0"sv, "\0::1"sv })
        EXPECT_FALSE(IPAddress::Parse(text));
    EXPECT_FALSE(IPAddress::Parse(std::string(INET6_ADDRSTRLEN, '1')));
    EXPECT_FALSE(IPAddress::Parse(std::string(1024, '1')));
}

TEST(IPAddress, ParsesStringViewsWithoutNullTermination)
{
    const std::string input = "127.0.0.1junk";
    EXPECT_EQ(IPAddress::Parse(std::string_view(input.data(), 9)),
        IPAddress::Parse("127.0.0.1"));
}

TEST(IPAddress, FactoryAddressesHaveExpectedBytesAndVersions)
{
    for (auto version : { IPVersion::IPv4, IPVersion::IPv6 })
    {
        const auto any = IPAddress::Any(version);
        const auto loopback = IPAddress::Loopback(version);
        EXPECT_EQ(any.GetVersion(), version);
        EXPECT_EQ(loopback.GetVersion(), version);
        EXPECT_EQ(any.ToString(), version == IPVersion::IPv4 ? "0.0.0.0" : "::");
        EXPECT_EQ(loopback.ToString(), version == IPVersion::IPv4 ? "127.0.0.1" : "::1");
        for (auto byte : any.SpanBytes()) EXPECT_EQ(byte, 0);
        EXPECT_NE(any, loopback);
    }
}

TEST(IPAddress, FromBytesCopiesNetworkOrderBytes)
{
    std::array<std::uint8_t, 4> v4{ 192, 0, 2, 42 };
    const auto address4 = IPAddress::FromBytes(v4, IPVersion::IPv4);
    ASSERT_TRUE(address4);
    v4.fill(0);
    EXPECT_EQ(address4->ToString(), "192.0.2.42");

    std::array<std::uint8_t, 16> v6{ 0x20, 0x01, 0x0d, 0xb8 };
    v6[15] = 42;
    const auto address6 = IPAddress::FromBytes(v6, IPVersion::IPv6);
    ASSERT_TRUE(address6);
    v6.fill(0);
    EXPECT_EQ(address6->ToString(), "2001:db8::2a");
}

TEST(IPAddress, FromBytesRejectsInvalidVersionsAndLengths)
{
    const std::array<std::uint8_t, 17> bytes{};
    for (auto version : { IPVersion::IPv4, IPVersion::IPv6 })
        for (std::size_t size = 0; size <= bytes.size(); ++size)
            EXPECT_EQ(IPAddress::FromBytes(std::span(bytes).first(size), version).has_value(),
                size == (version == IPVersion::IPv4 ? 4u : 16u));
    EXPECT_FALSE(IPAddress::FromBytes(std::span(bytes).first(4), static_cast<IPVersion>(42)));
}

TEST(IPAddress, ComparesByVersionThenNetworkBytes)
{
    const auto v4 = IPAddress::Parse("192.0.2.9");
    const auto v4Next = IPAddress::Parse("192.0.2.10");
    const auto v6 = IPAddress::Parse("2001:db8::ff");
    const auto v6Next = IPAddress::Parse("2001:db8::100");
    ASSERT_TRUE(v4 && v4Next && v6 && v6Next);
    EXPECT_LT(*v4, *v4Next);
    EXPECT_LT(*v6, *v6Next);
    EXPECT_LT(*v4Next, *v6);
    EXPECT_GT(*v6Next, *v6);
    EXPECT_EQ(*v6 <=> *v6, std::strong_ordering::equal);
    EXPECT_NE(IPAddress::Any(IPVersion::IPv4), IPAddress::Any(IPVersion::IPv6));
}

TEST(IPAddress, EquivalentSpellingsWorkAsOrderedAndHashedKeys)
{
    const auto compressed = IPAddress::Parse("2001:db8::1");
    const auto expanded = IPAddress::Parse("2001:0DB8:0:0:0:0:0:1");
    ASSERT_TRUE(compressed && expanded);
    EXPECT_EQ(*compressed, *expanded);
    EXPECT_EQ(compressed->Hash(), expanded->Hash());
    EXPECT_EQ(std::hash<IPAddress>{}(*compressed), compressed->Hash());
    const std::set<IPAddress> ordered{ *compressed, *expanded, IPAddress::Any(IPVersion::IPv4) };
    const std::unordered_set<IPAddress> hashed{ *compressed, *expanded, IPAddress::Any(IPVersion::IPv4) };
    EXPECT_EQ(ordered.size(), 2u);
    EXPECT_EQ(hashed.size(), 2u);
    EXPECT_TRUE(hashed.contains(*expanded));
}
