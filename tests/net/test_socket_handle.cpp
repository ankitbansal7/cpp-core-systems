#include "net/socket_handle.hpp"
#include "net/socket_runtime.hpp"
#include <gtest/gtest.h>
#include <functional>
#include <type_traits>
#include <utility>

static_assert(!std::is_copy_constructible_v<SocketHandle>);
static_assert(!std::is_copy_assignable_v<SocketHandle>);
static_assert(std::is_nothrow_move_constructible_v<SocketHandle>);
static_assert(std::is_nothrow_move_assignable_v<SocketHandle>);
static_assert(std::is_nothrow_destructible_v<SocketHandle>);

namespace
{
class SocketHandleTest : public testing::Test
{
protected:
    void SetUp() override { ASSERT_TRUE(runtime) << runtime.GetError(); }

    static SocketHandle CreateSocket()
    {
        // Unbound sockets require neither network access nor a free port.
        return SocketHandle{ ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP) };
    }

    static int SocketType(SocketHandle::NativeHandle handle)
    {
        int type = 0;
        socklen_t size = sizeof(type);
#ifdef _WIN32
        return ::getsockopt(handle, SOL_SOCKET, SO_TYPE, reinterpret_cast<char*>(&type), &size);
#else
        return ::getsockopt(handle, SOL_SOCKET, SO_TYPE, &type, &size);
#endif
    }

    static void ExpectOpen(SocketHandle::NativeHandle handle)
    {
        EXPECT_EQ(SocketType(handle), 0);
    }

    static void ExpectClosed(SocketHandle::NativeHandle handle)
    {
        const auto result = SocketType(handle);
        const auto error = SocketError::Last();
        EXPECT_EQ(result, -1);
#ifdef _WIN32
        EXPECT_EQ(error.Code(), WSAENOTSOCK);
#else
        EXPECT_EQ(error.Code(), EBADF);
#endif
    }

    SocketRuntime runtime;
};
}

TEST_F(SocketHandleTest, DefaultAndFactoryHandlesAreInvalid)
{
    SocketHandle empty;
    auto invalid = SocketHandle::Invalid();
    EXPECT_FALSE(empty.IsValid());
    EXPECT_FALSE(static_cast<bool>(empty));
    EXPECT_EQ(empty.Get(), SocketHandle::InvalidHandle);
    EXPECT_EQ(empty, invalid);
    EXPECT_EQ(empty.Release(), SocketHandle::InvalidHandle);
    empty.Close();
    empty.Close();
    empty.Reset();
    EXPECT_FALSE(empty);
}

TEST_F(SocketHandleTest, CloseReleasesNativeSocketAndIsIdempotent)
{
    auto handle = CreateSocket();
    ASSERT_TRUE(handle);
    const auto native = handle.Get();
    EXPECT_TRUE(handle.IsValid());
    ExpectOpen(native);
    handle.Close();
    EXPECT_EQ(handle.Get(), SocketHandle::InvalidHandle);
    EXPECT_FALSE(handle);
    ExpectClosed(native);
    handle.Close();
    ExpectClosed(native);
}

TEST_F(SocketHandleTest, DestructorClosesOwnedSocket)
{
    auto native = SocketHandle::InvalidHandle;
    {
        auto handle = CreateSocket();
        ASSERT_TRUE(handle);
        native = handle.Get();
    }
    ExpectClosed(native);
}

TEST_F(SocketHandleTest, ReleaseTransfersOwnershipWithoutClosing)
{
    auto native = SocketHandle::InvalidHandle;
    {
        auto handle = CreateSocket();
        ASSERT_TRUE(handle);
        native = handle.Release();
        EXPECT_FALSE(handle);
        EXPECT_EQ(handle.Release(), SocketHandle::InvalidHandle);
        ExpectOpen(native);
    }
    SocketHandle owner{ native };
    ExpectOpen(native);
    owner.Close();
    ExpectClosed(native);
}

TEST_F(SocketHandleTest, MoveConstructionTransfersOwnership)
{
    SocketHandle moved;
    auto native = SocketHandle::InvalidHandle;
    {
        auto original = CreateSocket();
        ASSERT_TRUE(original);
        native = original.Get();
        SocketHandle constructed{ std::move(original) };
        EXPECT_FALSE(original);
        EXPECT_EQ(constructed.Get(), native);
        moved = std::move(constructed);
        EXPECT_FALSE(constructed);
    }
    ExpectOpen(native);
    moved.Close();
    ExpectClosed(native);
    SocketHandle invalid{ SocketHandle::Invalid() };
    EXPECT_FALSE(invalid);
}

TEST_F(SocketHandleTest, MoveAssignmentClosesDestinationAndTransfersSource)
{
    auto destination = CreateSocket();
    auto source = CreateSocket();
    ASSERT_TRUE(destination && source);
    const auto oldDestination = destination.Get();
    const auto sourceNative = source.Get();
    auto& returned = (destination = std::move(source));
    EXPECT_EQ(&returned, &destination);
    EXPECT_FALSE(source);
    EXPECT_EQ(destination.Get(), sourceNative);
    ExpectClosed(oldDestination);
    ExpectOpen(sourceNative);
}

TEST_F(SocketHandleTest, AssignmentFromInvalidAndSelfMoveAreSafe)
{
    auto handle = CreateSocket();
    ASSERT_TRUE(handle);
    const auto native = handle.Get();
    auto& alias = handle;
    handle = std::move(alias);
    EXPECT_EQ(handle.Get(), native);
    ExpectOpen(native);
    SocketHandle invalid;
    handle = std::move(invalid);
    EXPECT_FALSE(handle);
    EXPECT_FALSE(invalid);
    ExpectClosed(native);
}

TEST_F(SocketHandleTest, ResetClosesOldSocketAndAdoptsReplacement)
{
    auto handle = CreateSocket();
    auto replacement = CreateSocket();
    ASSERT_TRUE(handle && replacement);
    const auto old = handle.Get();
    const auto next = replacement.Release();
    handle.Reset(next);
    EXPECT_EQ(handle.Get(), next);
    ExpectClosed(old);
    ExpectOpen(next);
    handle.Reset(next);
    ExpectOpen(next);
    handle.Reset();
    EXPECT_FALSE(handle);
    ExpectClosed(next);
    auto fresh = CreateSocket();
    ASSERT_TRUE(fresh);
    const auto freshNative = fresh.Get();
    handle.Reset(fresh.Release());
    EXPECT_EQ(handle.Get(), freshNative);
    ExpectOpen(freshNative);
}

TEST_F(SocketHandleTest, MemberAndAdlSwapPreserveOwnership)
{
    auto first = CreateSocket();
    auto second = CreateSocket();
    ASSERT_TRUE(first && second);
    const auto a = first.Get();
    const auto b = second.Get();
    first.Swap(second);
    EXPECT_EQ(first.Get(), b);
    EXPECT_EQ(second.Get(), a);
    using std::swap;
    swap(first, second);
    EXPECT_EQ(first.Get(), a);
    EXPECT_EQ(second.Get(), b);
    first.Swap(first);
    EXPECT_EQ(first.Get(), a);
    SocketHandle empty;
    swap(first, empty);
    EXPECT_FALSE(first);
    EXPECT_EQ(empty.Get(), a);
    ExpectOpen(a);
    ExpectOpen(b);
}

TEST_F(SocketHandleTest, ComparisonAndHashUseNativeIdentity)
{
    auto first = CreateSocket();
    auto second = CreateSocket();
    ASSERT_TRUE(first && second);
    EXPECT_EQ(first, first);
    EXPECT_NE(first, second);
    EXPECT_EQ(first <=> second, first.Get() <=> second.Get());
    EXPECT_EQ(std::hash<SocketHandle>{}(first), std::hash<SocketHandle::NativeHandle>{}(first.Get()));
    const auto invalid = SocketHandle::Invalid();
    EXPECT_EQ(std::hash<SocketHandle>{}(invalid),
        std::hash<SocketHandle::NativeHandle>{}(SocketHandle::InvalidHandle));
    EXPECT_EQ(first <=> invalid, first.Get() <=> SocketHandle::InvalidHandle);
}
