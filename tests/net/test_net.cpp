#include "net/socket_endpoint.hpp"
#include "net/socket_error.hpp"
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

TEST(SocketError, ClassifiesGenericConditions)
{
    EXPECT_TRUE(SocketError{ std::make_error_code(std::errc::operation_would_block) }.IsWouldBlock());
    EXPECT_TRUE(SocketError{ std::make_error_code(std::errc::interrupted) }.IsInterrupted());
    EXPECT_TRUE(SocketError{ std::make_error_code(std::errc::connection_reset) }.IsDisconnected());
    EXPECT_TRUE(SocketError{ std::make_error_code(std::errc::connection_refused) }.IsConnectionRefused());
    EXPECT_TRUE(SocketError{ std::make_error_code(std::errc::timed_out) }.IsTransient());
    EXPECT_TRUE(SocketError{ std::make_error_code(std::errc::address_in_use) }.IsAddressInUse());
    EXPECT_TRUE(SocketError{ std::make_error_code(std::errc::connection_aborted) }.IsDisconnected());
    EXPECT_TRUE(SocketError{ std::make_error_code(std::errc::broken_pipe) }.IsDisconnected());
    EXPECT_TRUE(SocketError{ std::make_error_code(std::errc::not_connected) }.IsDisconnected());
    EXPECT_FALSE(SocketError::Success().IsTransient());
    EXPECT_FALSE(SocketError::Success().IsDisconnected());
}

#ifdef _WIN32
TEST(SocketError, ClassifiesWinsockErrors)
{
    const std::pair<int, std::errc> cases[]{
        { WSAEWOULDBLOCK, std::errc::operation_would_block },
        { WSAEWOULDBLOCK, std::errc::resource_unavailable_try_again },
        { WSAEINTR, std::errc::interrupted },
        { WSAECONNRESET, std::errc::connection_reset },
        { WSAECONNREFUSED, std::errc::connection_refused },
        { WSAETIMEDOUT, std::errc::timed_out },
        { WSAEADDRINUSE, std::errc::address_in_use },
        { WSAECONNABORTED, std::errc::connection_aborted },
        { WSAENOTCONN, std::errc::not_connected },
    };

    for (const auto& [code, condition] : cases)
    {
        const SocketError error{ code };
        EXPECT_EQ(error, condition);
        EXPECT_FALSE(error != condition);
        EXPECT_NE(error, std::errc::invalid_argument);
        EXPECT_EQ(error.Code(), code);
        EXPECT_EQ(error.ErrorCode(), std::error_code(code, std::system_category()));
        // Identical numbers from other categories must not be treated as Winsock errors.
        EXPECT_NE((SocketError{ std::error_code(code, std::generic_category()) }), condition);
    }

    EXPECT_TRUE(SocketError{ WSAEWOULDBLOCK }.IsWouldBlock());
    EXPECT_TRUE(SocketError{ WSAEINTR }.IsInterrupted());
    EXPECT_TRUE(SocketError{ WSAECONNRESET }.IsConnectionReset());
    EXPECT_TRUE(SocketError{ WSAECONNREFUSED }.IsConnectionRefused());
    EXPECT_TRUE(SocketError{ WSAETIMEDOUT }.IsTimedOut());
    EXPECT_TRUE(SocketError{ WSAEADDRINUSE }.IsAddressInUse());
    EXPECT_TRUE(SocketError{ WSAECONNABORTED }.IsConnectionAborted());
    EXPECT_TRUE(SocketError{ WSAEWOULDBLOCK }.IsTransient());
    EXPECT_TRUE(SocketError{ WSAEINTR }.IsTransient());
    EXPECT_TRUE(SocketError{ WSAETIMEDOUT }.IsTransient());
    EXPECT_TRUE(SocketError{ WSAECONNRESET }.IsDisconnected());
    EXPECT_TRUE(SocketError{ WSAECONNABORTED }.IsDisconnected());
    EXPECT_TRUE(SocketError{ WSAENOTCONN }.IsDisconnected());
    EXPECT_FALSE(SocketError{ WSAEINVAL }.IsWouldBlock());
    EXPECT_FALSE(SocketError{ WSAECONNREFUSED }.IsTransient());
    EXPECT_FALSE(SocketError{ WSAEWOULDBLOCK }.IsDisconnected());

    ::WSASetLastError(WSAEWOULDBLOCK);
    EXPECT_TRUE(SocketError::Last().IsWouldBlock());
    ::WSASetLastError(0);
}
#endif
