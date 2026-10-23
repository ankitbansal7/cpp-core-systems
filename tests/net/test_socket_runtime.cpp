#include "net/socket_runtime.hpp"
#include "net/socket_handle.hpp"
#include <gtest/gtest.h>
#include <type_traits>

static_assert(!std::is_copy_constructible_v<SocketRuntime>);
static_assert(!std::is_copy_assignable_v<SocketRuntime>);
static_assert(!std::is_move_constructible_v<SocketRuntime>);
static_assert(!std::is_move_assignable_v<SocketRuntime>);
static_assert(std::is_nothrow_default_constructible_v<SocketRuntime>);
static_assert(std::is_nothrow_destructible_v<SocketRuntime>);

TEST(SocketRuntime, InitializesAndReportsSuccess)
{
    SocketRuntime runtime;
    ASSERT_TRUE(runtime.IsInitialized()) << runtime.GetError();
    EXPECT_TRUE(static_cast<bool>(runtime));
    EXPECT_TRUE(runtime.GetError().IsSuccess());
    EXPECT_EQ(runtime.GetError().Code(), 0);
    SocketHandle socket{ ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP) };
    EXPECT_TRUE(socket) << SocketError::Last();
}

TEST(SocketRuntime, NestedRuntimeDestructionKeepsOuterRuntimeUsable)
{
    SocketRuntime outer;
    ASSERT_TRUE(outer);
    SocketHandle existing{ ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP) };
    ASSERT_TRUE(existing);
    {
        SocketRuntime inner;
        ASSERT_TRUE(inner);
        SocketHandle temporary{ ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP) };
        EXPECT_TRUE(temporary);
    }
    // A surviving socket and a newly created socket must both remain usable.
    int type = 0;
    socklen_t length = sizeof(type);
#ifdef _WIN32
    EXPECT_EQ(::getsockopt(existing.Get(), SOL_SOCKET, SO_TYPE,
        reinterpret_cast<char*>(&type), &length), 0);
#else
    EXPECT_EQ(::getsockopt(existing.Get(), SOL_SOCKET, SO_TYPE, &type, &length), 0);
#endif
    EXPECT_EQ(type, SOCK_STREAM);
    SocketHandle afterInner{ ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP) };
    EXPECT_TRUE(afterInner) << SocketError::Last();
}

TEST(SocketRuntime, CanInitializeAgainAfterDestruction)
{
    for (int iteration = 0; iteration < 3; ++iteration)
    {
        SocketRuntime runtime;
        ASSERT_TRUE(runtime);
        SocketHandle socket{ ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP) };
        ASSERT_TRUE(socket) << SocketError::Last();
    }
#ifdef _WIN32
    // No runtime is alive here: every successful WSAStartup must be balanced.
    SocketHandle withoutRuntime{ ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP) };
    const auto error = SocketError::Last();
    EXPECT_FALSE(withoutRuntime);
    EXPECT_EQ(error.Code(), WSANOTINITIALISED);
#endif
}
