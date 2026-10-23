#include "net/tcp_listener.hpp"
#include "net/socket_runtime.hpp"
#include "socket_test_support.hpp"

#include <array>
#include <type_traits>
#include <utility>

static_assert(!std::is_copy_constructible_v<TcpListener>);
static_assert(!std::is_copy_assignable_v<TcpListener>);
static_assert(std::is_nothrow_move_constructible_v<TcpListener>);
static_assert(std::is_nothrow_move_assignable_v<TcpListener>);

namespace
{
    class TcpListenerTest : public testing::TestWithParam<IPVersion>
    {
    protected:
        void SetUp() override
        {
            ASSERT_TRUE(runtime) << runtime.GetError();
            const auto bound = listener.Bind({ IPAddress::Loopback(GetParam()), 0 });
            if (!bound && GetParam() == IPVersion::IPv6)
            {
                GTEST_SKIP() << "IPv6 loopback unavailable: " << bound.Error();
            }
            ASSERT_TRUE(bound) << bound.Error();
            const auto local = listener.GetLocalEndpoint();
            ASSERT_TRUE(local) << local.Error();
            endpoint = local.Value();
        }

        static void Exchange(TcpSocket& sender, TcpSocket& receiver)
        {
            const std::array<std::byte, 5> payload{
                std::byte{0}, std::byte{42}, std::byte{128}, std::byte{255}, std::byte{1} };
            std::size_t sent = 0;
            while (sent < payload.size())
            {
                const auto result = sender.Send(std::span{payload}.subspan(sent));
                ASSERT_TRUE(result) << result.Error();
                ASSERT_GT(result.Value(), 0u);
                sent += result.Value();
            }
            std::array<std::byte, 5> received{};
            std::size_t count = 0;
            while (count < received.size())
            {
                const auto result = receiver.Receive(std::span{received}.subspan(count));
                ASSERT_TRUE(result) << result.Error();
                ASSERT_GT(result.Value(), 0u);
                count += result.Value();
            }
            EXPECT_EQ(received, payload);
        }

        SocketRuntime runtime;
        TcpListener listener;
        SocketEndpoint endpoint;
    };
}

TEST(TcpListener, DefaultAndClosedOperationsReturnErrors)
{
    TcpListener listener;
    EXPECT_FALSE(listener);
    EXPECT_FALSE(listener.Listen());
    EXPECT_FALSE(listener.Accept());
    EXPECT_FALSE(listener.GetLocalEndpoint());
    EXPECT_EQ(listener.GetNativeHandle(), SocketHandle::InvalidHandle);
    listener.Close();
    listener.Close();
}

TEST_P(TcpListenerTest, BindListenAndAcceptHaveDistinctRoles)
{
    EXPECT_TRUE(listener);
    EXPECT_EQ(endpoint.GetAddress(), IPAddress::Loopback(GetParam()));
    EXPECT_NE(endpoint.GetPort(), 0);
    EXPECT_FALSE(listener.Accept()); // Bound but not listening: fails immediately.
    EXPECT_FALSE(listener.Listen(0));
    EXPECT_FALSE(listener.Listen(-1));
    ASSERT_TRUE(listener.Listen(4));
    TcpSocket client;
    ASSERT_TRUE(client.Connect(endpoint));
    ASSERT_TRUE(socket_test::SetTimeouts(client.GetNativeHandle()));
    ASSERT_TRUE(socket_test::IsReadable(listener.GetNativeHandle()));
    auto accepted = listener.Accept();
    ASSERT_TRUE(accepted) << accepted.Error();
    auto server = std::move(accepted).Value();
    ASSERT_TRUE(socket_test::SetTimeouts(server.GetNativeHandle()));
    const auto serverLocal = server.GetLocalEndpoint();
    const auto serverPeer = server.GetPeerEndpoint();
    const auto clientLocal = client.GetLocalEndpoint();
    ASSERT_TRUE(serverLocal);
    ASSERT_TRUE(serverPeer);
    ASSERT_TRUE(clientLocal);
    EXPECT_EQ(serverLocal.Value(), endpoint);
    EXPECT_EQ(serverPeer.Value(), clientLocal.Value());
    ASSERT_NO_FATAL_FAILURE(Exchange(client, server));
    ASSERT_NO_FATAL_FAILURE(Exchange(server, client));
    const auto shutdown = server.Shutdown(SocketShutdown::Send);
    ASSERT_TRUE(shutdown) << shutdown.Error();
    std::array<std::byte, 1> buffer{};
    const auto eof = client.Receive(buffer);
    ASSERT_TRUE(eof) << eof.Error();
    EXPECT_EQ(eof.Value(), 0u);
}

TEST_P(TcpListenerTest, AcceptsMultipleConnectionsAndAcceptedSocketsOutliveListener)
{
    ASSERT_TRUE(listener.Listen());
    std::array<TcpSocket, 2> clients;
    std::array<TcpSocket, 2> servers;
    for (std::size_t index = 0; index < clients.size(); ++index)
    {
        ASSERT_TRUE(clients[index].Connect(endpoint));
        ASSERT_TRUE(socket_test::SetTimeouts(clients[index].GetNativeHandle()));
        ASSERT_TRUE(socket_test::IsReadable(listener.GetNativeHandle()));
        auto accepted = listener.Accept();
        ASSERT_TRUE(accepted) << accepted.Error();
        servers[index] = std::move(accepted).Value();
        ASSERT_TRUE(socket_test::SetTimeouts(servers[index].GetNativeHandle()));
    }
    listener.Close();
    EXPECT_FALSE(listener);
    for (std::size_t index = 0; index < clients.size(); ++index)
    {
        ASSERT_NO_FATAL_FAILURE(Exchange(clients[index], servers[index]));
        ASSERT_NO_FATAL_FAILURE(Exchange(servers[index], clients[index]));
    }
}

TEST_P(TcpListenerTest, FailedBindIsReusableAndExistingListenerIsPreserved)
{
    TcpListener other;
    const auto conflict = other.Bind(endpoint);
    ASSERT_FALSE(conflict);
    EXPECT_TRUE(conflict.Error().IsAddressInUse()) << conflict.Error();
    EXPECT_FALSE(other);
    ASSERT_TRUE(other.Bind({ IPAddress::Loopback(GetParam()), 0 }));
    const auto native = listener.GetNativeHandle();
    EXPECT_FALSE(listener.Bind(endpoint));
    EXPECT_EQ(listener.GetNativeHandle(), native);
    EXPECT_TRUE(listener.Listen());
}

TEST_P(TcpListenerTest, MovesKeepListeningAndCloseAllowsRebinding)
{
    ASSERT_TRUE(listener.Listen());
    const auto native = listener.GetNativeHandle();
    TcpListener moved{ std::move(listener) };
    EXPECT_FALSE(listener);
    EXPECT_EQ(moved.GetNativeHandle(), native);
    TcpListener assigned;
    ASSERT_TRUE(assigned.Bind({ IPAddress::Loopback(GetParam()), 0 }));
    assigned = std::move(moved);
    EXPECT_FALSE(moved);
    EXPECT_EQ(assigned.GetNativeHandle(), native);
    TcpSocket client;
    ASSERT_TRUE(client.Connect(endpoint));
    ASSERT_TRUE(socket_test::IsReadable(assigned.GetNativeHandle()));
    auto accepted = assigned.Accept();
    ASSERT_TRUE(accepted) << accepted.Error();
    assigned.Close();
    assigned.Close();
    EXPECT_FALSE(assigned);
    ASSERT_TRUE(assigned.Bind({ IPAddress::Loopback(GetParam()), 0 }));
    EXPECT_TRUE(assigned.Listen());
}

INSTANTIATE_TEST_SUITE_P(Loopback, TcpListenerTest,
    testing::Values(IPVersion::IPv4, IPVersion::IPv6),
    [](const testing::TestParamInfo<IPVersion>& info)
    {
        return info.param == IPVersion::IPv4 ? "IPv4" : "IPv6";
    });
