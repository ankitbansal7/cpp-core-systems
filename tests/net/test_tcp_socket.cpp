#include "net/tcp_socket.hpp"
#include "net/socket_runtime.hpp"
#include "tcp_test_support.hpp"

#include <array>
#include <type_traits>
#include <utility>

static_assert(!std::is_copy_constructible_v<TcpSocket>);
static_assert(!std::is_copy_assignable_v<TcpSocket>);
static_assert(std::is_nothrow_move_constructible_v<TcpSocket>);
static_assert(std::is_nothrow_move_assignable_v<TcpSocket>);

namespace
{
    class TcpSocketTest : public testing::TestWithParam<IPVersion>
    {
    protected:
        void SetUp() override
        {
            ASSERT_TRUE(runtime) << runtime.GetError();
            const int family = GetParam() == IPVersion::IPv4 ? AF_INET : AF_INET6;
            listener.Reset(::socket(family, SOCK_STREAM, IPPROTO_TCP));
            if (!listener && GetParam() == IPVersion::IPv6)
            {
                GTEST_SKIP() << "IPv6 unavailable: " << SocketError::Last();
            }
            ASSERT_TRUE(listener) << SocketError::Last();
            const SocketEndpoint requested{ IPAddress::Loopback(GetParam()), 0 };
            const auto [storage, length] = requested.ToNative();
            const int bound = ::bind(listener.Get(), reinterpret_cast<const sockaddr*>(&storage), length);
            if (bound != 0 && GetParam() == IPVersion::IPv6)
            {
                GTEST_SKIP() << "IPv6 loopback unavailable: " << SocketError::Last();
            }
            ASSERT_EQ(bound, 0) << SocketError::Last();
            sockaddr_storage local{};
            socklen_t localLength = sizeof(local);
            ASSERT_EQ(::getsockname(listener.Get(), reinterpret_cast<sockaddr*>(&local), &localLength), 0);
            const auto parsed = SocketEndpoint::FromNative(reinterpret_cast<sockaddr*>(&local), localLength);
            ASSERT_TRUE(parsed);
            endpoint = *parsed;
        }

        void ConnectAndAccept(TcpSocket& client, SocketHandle& peer)
        {
            ASSERT_EQ(::listen(listener.Get(), 4), 0);
            const auto connected = client.Connect(endpoint);
            ASSERT_TRUE(connected) << connected.Error();
            ASSERT_TRUE(tcp_test::SetTimeouts(client.GetNativeHandle()));
            ASSERT_TRUE(tcp_test::IsReadable(listener.Get()));
            peer.Reset(::accept(listener.Get(), nullptr, nullptr));
            ASSERT_TRUE(peer) << SocketError::Last();
            ASSERT_TRUE(tcp_test::SetTimeouts(peer.Get()));
        }

        SocketRuntime runtime;
        SocketHandle listener;
        SocketEndpoint endpoint;
    };
}

TEST(TcpSocket, EmptyAndClosedOperationsReturnErrors)
{
    TcpSocket socket;
    EXPECT_FALSE(socket);
    EXPECT_EQ(socket.GetNativeHandle(), SocketHandle::InvalidHandle);
    std::array<std::byte, 1> buffer{};
    EXPECT_FALSE(socket.Send(buffer));
    EXPECT_FALSE(socket.Receive(buffer));
    EXPECT_FALSE(socket.Shutdown());
    EXPECT_FALSE(socket.GetLocalEndpoint());
    EXPECT_FALSE(socket.GetPeerEndpoint());
    socket.Close();
    socket.Close();
    EXPECT_FALSE(socket);
}

TEST_P(TcpSocketTest, ExchangesBinaryDataAndReportsEndpoints)
{
    TcpSocket client;
    SocketHandle peer;
    ASSERT_NO_FATAL_FAILURE(ConnectAndAccept(client, peer));
    const auto remote = client.GetPeerEndpoint();
    ASSERT_TRUE(remote) << remote.Error();
    EXPECT_EQ(remote.Value(), endpoint);
    const auto local = client.GetLocalEndpoint();
    ASSERT_TRUE(local) << local.Error();
    EXPECT_EQ(local.Value().GetAddress(), IPAddress::Loopback(GetParam()));
    EXPECT_NE(local.Value().GetPort(), 0);

    const std::array<std::byte, 4> payload{ std::byte{0}, std::byte{1}, std::byte{128}, std::byte{255} };
    std::size_t sent = 0;
    while (sent < payload.size())
    {
        const auto result = client.Send(std::span{payload}.subspan(sent));
        ASSERT_TRUE(result) << result.Error();
        ASSERT_GT(result.Value(), 0u);
        sent += result.Value();
    }
    std::array<std::byte, 4> received{};
    std::size_t count = 0;
    while (count < received.size())
    {
        const auto bytes = ::recv(peer.Get(), reinterpret_cast<char*>(received.data() + count),
            static_cast<int>(received.size() - count), 0);
        ASSERT_GT(bytes, 0) << SocketError::Last();
        count += static_cast<std::size_t>(bytes);
    }
    EXPECT_EQ(received, payload);
    // Tiny buffers force the caller to assemble a stream over multiple receives.
    ASSERT_EQ(::send(peer.Get(), reinterpret_cast<const char*>(payload.data()), 4, 0), 4);
    std::array<std::byte, 4> reply{};
    for (auto& byte : reply)
    {
        const auto result = client.Receive(std::span{ &byte, 1 });
        ASSERT_TRUE(result) << result.Error();
        ASSERT_EQ(result.Value(), 1u);
    }
    EXPECT_EQ(reply, payload);
}

TEST_P(TcpSocketTest, EmptyBuffersAreNoOpsAndSendShutdownStillAllowsReceiving)
{
    TcpSocket client;
    SocketHandle peer;
    ASSERT_NO_FATAL_FAILURE(ConnectAndAccept(client, peer));
    const auto sent = client.Send({});
    const auto received = client.Receive({});
    ASSERT_TRUE(sent);
    ASSERT_TRUE(received);
    EXPECT_EQ(sent.Value(), 0u);
    EXPECT_EQ(received.Value(), 0u);
    ASSERT_TRUE(client.Shutdown(SocketShutdown::Send));
    char byte = 0;
    EXPECT_EQ(::recv(peer.Get(), &byte, 1, 0), 0);
    ASSERT_EQ(::send(peer.Get(), "x", 1, 0), 1);
#ifdef _WIN32
    ASSERT_EQ(::shutdown(peer.Get(), SD_SEND), 0);
#else
    ASSERT_EQ(::shutdown(peer.Get(), SHUT_WR), 0);
#endif
    std::array<std::byte, 1> buffer{};
    const auto reply = client.Receive(buffer);
    ASSERT_TRUE(reply) << reply.Error();
    EXPECT_EQ(reply.Value(), 1u);
    EXPECT_EQ(buffer[0], std::byte{'x'});
    const auto eof = client.Receive(buffer);
    ASSERT_TRUE(eof) << eof.Error();
    EXPECT_EQ(eof.Value(), 0u);
    const auto failedSend = client.Send(buffer);
    EXPECT_FALSE(failedSend); // Also verifies Linux does not terminate on SIGPIPE.
}

TEST_P(TcpSocketTest, FailedConnectLeavesSocketReusable)
{
    // The port stays reserved by a bound socket, but nothing is listening yet.
    TcpSocket client;
    const auto failed = client.Connect(endpoint);
    ASSERT_FALSE(failed);
    EXPECT_TRUE(failed.Error().IsConnectionRefused()) << failed.Error();
    EXPECT_FALSE(client);
    SocketHandle peer;
    ASSERT_NO_FATAL_FAILURE(ConnectAndAccept(client, peer));
    EXPECT_TRUE(client);
}

TEST_P(TcpSocketTest, MovesTransferOwnershipAndCloseAllowsReconnect)
{
    TcpSocket client;
    SocketHandle peer;
    ASSERT_NO_FATAL_FAILURE(ConnectAndAccept(client, peer));
    const auto native = client.GetNativeHandle();
    const auto duplicate = client.Connect(endpoint);
    EXPECT_FALSE(duplicate);
    EXPECT_EQ(client.GetNativeHandle(), native);
    EXPECT_FALSE(client.Shutdown(static_cast<SocketShutdown>(99)));
    TcpSocket moved{ std::move(client) };
    EXPECT_FALSE(client);
    EXPECT_EQ(moved.GetNativeHandle(), native);
    TcpSocket assigned;
    assigned = std::move(moved);
    EXPECT_FALSE(moved);
    EXPECT_EQ(assigned.GetNativeHandle(), native);
    assigned.Close();
    EXPECT_FALSE(assigned);
    char byte = 0;
    EXPECT_EQ(::recv(peer.Get(), &byte, 1, 0), 0);
    ASSERT_TRUE(assigned.Connect(endpoint));
    EXPECT_TRUE(assigned);
}

INSTANTIATE_TEST_SUITE_P(Loopback, TcpSocketTest,
    testing::Values(IPVersion::IPv4, IPVersion::IPv6),
    [](const testing::TestParamInfo<IPVersion>& info)
    {
        return info.param == IPVersion::IPv4 ? "IPv4" : "IPv6";
    });
