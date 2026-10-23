#include "net/udp_socket.hpp"
#include "net/socket_runtime.hpp"
#include "socket_test_support.hpp"

#include <array>
#include <type_traits>
#include <utility>
#include <vector>

static_assert(!std::is_copy_constructible_v<UdpSocket>);
static_assert(!std::is_copy_assignable_v<UdpSocket>);
static_assert(std::is_nothrow_move_constructible_v<UdpSocket>);
static_assert(std::is_nothrow_move_assignable_v<UdpSocket>);
static_assert(std::is_nothrow_destructible_v<UdpSocket>);

namespace
{
    class UdpSocketTest : public testing::TestWithParam<IPVersion>
    {
    protected:
        void SetUp() override
        {
            ASSERT_TRUE(runtime) << runtime.GetError();
            const auto bound = server.Bind({ IPAddress::Loopback(GetParam()), 0 });
            if (!bound && GetParam() == IPVersion::IPv6)
            {
                GTEST_SKIP() << "IPv6 loopback unavailable: " << bound.Error();
            }
            ASSERT_TRUE(bound) << bound.Error();
            const auto serverLocal = server.GetLocalEndpoint();
            ASSERT_TRUE(serverLocal) << serverLocal.Error();
            serverEndpoint = serverLocal.Value();
            ASSERT_NE(serverEndpoint.GetPort(), 0);
            ASSERT_TRUE(client.Bind({ IPAddress::Loopback(GetParam()), 0 }));
            const auto clientLocal = client.GetLocalEndpoint();
            ASSERT_TRUE(clientLocal) << clientLocal.Error();
            clientEndpoint = clientLocal.Value();
            ASSERT_TRUE(socket_test::SetTimeouts(server.GetNativeHandle()));
            ASSERT_TRUE(socket_test::SetTimeouts(client.GetNativeHandle()));
        }

        SocketRuntime runtime;
        UdpSocket server;
        UdpSocket client;
        SocketEndpoint serverEndpoint;
        SocketEndpoint clientEndpoint;
    };
}

TEST(UdpSocket, DefaultAndClosedReceiveOperationsReturnErrors)
{
    UdpSocket socket;
    EXPECT_FALSE(socket);
    EXPECT_EQ(socket.GetNativeHandle(), SocketHandle::InvalidHandle);
    std::array<std::byte, 1> buffer{};
    EXPECT_FALSE(socket.ReceiveFrom(buffer));
    EXPECT_FALSE(socket.GetLocalEndpoint());
    socket.Close();
    socket.Close();
    EXPECT_FALSE(socket);
}

TEST_P(UdpSocketTest, PreservesBinaryDatagramsAndSenderEndpoints)
{
    const std::array<std::byte, 4> first{
        std::byte{0}, std::byte{1}, std::byte{128}, std::byte{255} };
    const std::array<std::byte, 2> second{ std::byte{42}, std::byte{0} };
    const auto sentFirst = client.SendTo(first, serverEndpoint);
    const auto sentSecond = client.SendTo(second, serverEndpoint);
    ASSERT_TRUE(sentFirst) << sentFirst.Error();
    ASSERT_TRUE(sentSecond) << sentSecond.Error();
    EXPECT_EQ(sentFirst.Value(), first.size());
    EXPECT_EQ(sentSecond.Value(), second.size());
    std::array<std::byte, 32> buffer{};
    const auto receivedFirst = server.ReceiveFrom(buffer);
    ASSERT_TRUE(receivedFirst) << receivedFirst.Error();
    EXPECT_EQ(receivedFirst.Value().BytesReceived, first.size());
    EXPECT_EQ(receivedFirst.Value().Sender, clientEndpoint);
    EXPECT_TRUE(std::equal(first.begin(), first.end(), buffer.begin()));
    const auto receivedSecond = server.ReceiveFrom(buffer);
    ASSERT_TRUE(receivedSecond) << receivedSecond.Error();
    EXPECT_EQ(receivedSecond.Value().BytesReceived, second.size());
    EXPECT_EQ(receivedSecond.Value().Sender, clientEndpoint);
    EXPECT_TRUE(std::equal(second.begin(), second.end(), buffer.begin()));
    ASSERT_TRUE(server.SendTo(second, receivedSecond.Value().Sender));
    const auto reply = client.ReceiveFrom(buffer);
    ASSERT_TRUE(reply) << reply.Error();
    EXPECT_EQ(reply.Value().BytesReceived, second.size());
    EXPECT_EQ(reply.Value().Sender, serverEndpoint);
}

TEST_P(UdpSocketTest, FirstSendAutomaticallyCreatesAndBindsSender)
{
    UdpSocket sender;
    const std::array<std::byte, 1> payload{ std::byte{42} };
    const auto sent = sender.SendTo(payload, serverEndpoint);
    ASSERT_TRUE(sent) << sent.Error();
    ASSERT_TRUE(sender);
    ASSERT_TRUE(socket_test::SetTimeouts(sender.GetNativeHandle()));
    const auto local = sender.GetLocalEndpoint();
    ASSERT_TRUE(local) << local.Error();
    EXPECT_EQ(local.Value().GetVersion(), GetParam());
    EXPECT_NE(local.Value().GetPort(), 0);
    std::array<std::byte, 1> buffer{};
    const auto received = server.ReceiveFrom(buffer);
    ASSERT_TRUE(received) << received.Error();
    EXPECT_EQ(received.Value().Sender.GetAddress(), IPAddress::Loopback(GetParam()));
    EXPECT_EQ(received.Value().Sender.GetPort(), local.Value().GetPort());
    ASSERT_TRUE(server.SendTo(buffer, received.Value().Sender));
    const auto reply = sender.ReceiveFrom(buffer);
    ASSERT_TRUE(reply) << reply.Error();
    EXPECT_EQ(reply.Value().Sender, serverEndpoint);
    EXPECT_EQ(buffer, payload);
    EXPECT_FALSE(sender.Bind({ IPAddress::Loopback(GetParam()), 0 }));
}

TEST_P(UdpSocketTest, EmptyDatagramsAreMessagesAndDoNotSignalEOF)
{
    const auto sent = client.SendTo({}, serverEndpoint);
    ASSERT_TRUE(sent) << sent.Error();
    EXPECT_EQ(sent.Value(), 0u);
    const auto empty = server.ReceiveFrom({});
    ASSERT_TRUE(empty) << empty.Error();
    EXPECT_EQ(empty.Value().BytesReceived, 0u);
    EXPECT_EQ(empty.Value().Sender, clientEndpoint);
    ASSERT_TRUE(client.SendTo({}, serverEndpoint));
    std::array<std::byte, 1> buffer{};
    const auto anotherEmpty = server.ReceiveFrom(buffer);
    ASSERT_TRUE(anotherEmpty) << anotherEmpty.Error();
    EXPECT_EQ(anotherEmpty.Value().BytesReceived, 0u);
    buffer[0] = std::byte{42};
    ASSERT_TRUE(client.SendTo(buffer, serverEndpoint));
    buffer[0] = std::byte{0};
    const auto next = server.ReceiveFrom(buffer);
    ASSERT_TRUE(next) << next.Error();
    EXPECT_EQ(next.Value().BytesReceived, 1u);
    EXPECT_EQ(buffer[0], std::byte{42});
}

TEST_P(UdpSocketTest, TruncationReturnsMessageSizeAndDiscardsRemainder)
{
    const std::array<std::byte, 4> payload{};
    ASSERT_TRUE(client.SendTo(payload, serverEndpoint));
    std::array<std::byte, 2> small{};
    const auto truncated = server.ReceiveFrom(small);
    ASSERT_FALSE(truncated);
    EXPECT_EQ(truncated.Error(), std::errc::message_size);
    // An exact-fit datagram succeeds and has no bytes left over from the first.
    const std::array<std::byte, 2> next{ std::byte{42}, std::byte{255} };
    ASSERT_TRUE(client.SendTo(next, serverEndpoint));
    const auto received = server.ReceiveFrom(small);
    ASSERT_TRUE(received) << received.Error();
    EXPECT_EQ(received.Value().BytesReceived, next.size());
    EXPECT_EQ(small, next);
    ASSERT_TRUE(client.SendTo(next, serverEndpoint));
    const auto zeroCapacity = server.ReceiveFrom({});
    ASSERT_FALSE(zeroCapacity);
    EXPECT_EQ(zeroCapacity.Error(), std::errc::message_size);
    ASSERT_TRUE(client.SendTo({}, serverEndpoint));
    const auto empty = server.ReceiveFrom({});
    ASSERT_TRUE(empty) << empty.Error();
    EXPECT_EQ(empty.Value().BytesReceived, 0u);
}

TEST_P(UdpSocketTest, OversizedDatagramsFailAtomicallyAndSocketsRemainReusable)
{
    const std::vector<std::byte> oversized(65536);
    UdpSocket sender;
    const auto failedFirst = sender.SendTo(oversized, serverEndpoint);
    ASSERT_FALSE(failedFirst);
    EXPECT_EQ(failedFirst.Error(), std::errc::message_size);
    EXPECT_FALSE(sender);
    const auto native = client.GetNativeHandle();
    const auto failedExisting = client.SendTo(oversized, serverEndpoint);
    ASSERT_FALSE(failedExisting);
    EXPECT_EQ(failedExisting.Error(), std::errc::message_size);
    EXPECT_EQ(client.GetNativeHandle(), native);
    // The only received packet must be this valid message, not a partial send.
    const std::array<std::byte, 1> payload{ std::byte{42} };
    ASSERT_TRUE(sender.SendTo(payload, serverEndpoint));
    std::array<std::byte, 1> buffer{};
    const auto received = server.ReceiveFrom(buffer);
    ASSERT_TRUE(received) << received.Error();
    EXPECT_EQ(received.Value().BytesReceived, 1u);
    EXPECT_EQ(buffer, payload);
}

TEST_P(UdpSocketTest, FailedBindIsReusableAndMovesTransferBoundSocket)
{
    UdpSocket other;
    const auto conflict = other.Bind(serverEndpoint);
    ASSERT_FALSE(conflict);
    EXPECT_TRUE(conflict.Error().IsAddressInUse()) << conflict.Error();
    EXPECT_FALSE(other);
    ASSERT_TRUE(other.Bind({ IPAddress::Loopback(GetParam()), 0 }));
    const auto native = server.GetNativeHandle();
    EXPECT_FALSE(server.Bind(serverEndpoint));
    EXPECT_EQ(server.GetNativeHandle(), native);
    UdpSocket moved{ std::move(server) };
    EXPECT_FALSE(server);
    EXPECT_EQ(moved.GetNativeHandle(), native);
    other = std::move(moved); // Closes other's previously bound socket.
    EXPECT_FALSE(moved);
    EXPECT_EQ(other.GetNativeHandle(), native);
    const std::array<std::byte, 1> payload{ std::byte{42} };
    ASSERT_TRUE(client.SendTo(payload, serverEndpoint));
    std::array<std::byte, 1> buffer{};
    const auto received = other.ReceiveFrom(buffer);
    ASSERT_TRUE(received) << received.Error();
    EXPECT_EQ(buffer, payload);
    other.Close();
    other.Close();
    EXPECT_FALSE(other);
    ASSERT_TRUE(other.Bind({ IPAddress::Loopback(GetParam()), 0 }));
}

TEST_P(UdpSocketTest, ExchangesWithNativeDatagramSocket)
{
    const int family = GetParam() == IPVersion::IPv4 ? AF_INET : AF_INET6;
    SocketHandle native{ ::socket(family, SOCK_DGRAM, IPPROTO_UDP) };
    ASSERT_TRUE(native) << SocketError::Last();
    ASSERT_TRUE(socket_test::SetTimeouts(native.Get()));
    const auto [storage, length] = serverEndpoint.ToNative();
    ASSERT_EQ(::sendto(native.Get(), "udp", 3, 0,
        reinterpret_cast<const sockaddr*>(&storage), length), 3);
    std::array<std::byte, 3> buffer{};
    const auto received = server.ReceiveFrom(buffer);
    ASSERT_TRUE(received) << received.Error();
    EXPECT_EQ(received.Value().BytesReceived, 3u);
    EXPECT_EQ(buffer[0], std::byte{'u'});
    EXPECT_EQ(buffer[1], std::byte{'d'});
    EXPECT_EQ(buffer[2], std::byte{'p'});
    ASSERT_TRUE(server.SendTo(buffer, received.Value().Sender));
    std::array<char, 3> reply{};
    EXPECT_EQ(::recvfrom(native.Get(), reply.data(), 3, 0, nullptr, nullptr), 3);
    EXPECT_EQ((std::string_view{ reply.data(), reply.size() }), "udp");
}

INSTANTIATE_TEST_SUITE_P(Loopback, UdpSocketTest,
    testing::Values(IPVersion::IPv4, IPVersion::IPv6),
    [](const testing::TestParamInfo<IPVersion>& info)
    {
        return info.param == IPVersion::IPv4 ? "IPv4" : "IPv6";
    });
