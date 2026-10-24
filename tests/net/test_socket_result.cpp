#include "net/socket_result.hpp"
#include "net/socket_handle.hpp"

#include <gtest/gtest.h>
#include <memory>
#include <type_traits>
#include <utility>

using namespace net;

static_assert(!std::is_default_constructible_v<SocketResult<int>>);
static_assert(std::is_copy_constructible_v<SocketResult<int>>);
static_assert(std::is_copy_assignable_v<SocketResult<int>>);
static_assert(!std::is_copy_constructible_v<SocketResult<SocketHandle>>);
static_assert(!std::is_copy_assignable_v<SocketResult<SocketHandle>>);
static_assert(std::is_nothrow_move_constructible_v<SocketResult<SocketHandle>>);
static_assert(std::is_nothrow_move_assignable_v<SocketResult<SocketHandle>>);
static_assert(std::is_same_v<decltype(std::declval<SocketResult<int>&>().Value()), int&>);
static_assert(std::is_same_v<decltype(std::declval<const SocketResult<int>&>().Value()), const int&>);
static_assert(std::is_same_v<decltype(std::declval<SocketResult<int>&&>().Value()), int&&>);
static_assert(std::is_same_v<decltype(std::declval<const SocketResult<int>&&>().Value()), const int&&>);

static_assert(noexcept(SocketResult<int>::Success(1)));
static_assert(noexcept(SocketResult<int>::Failure(SocketError{ 1 })));
static_assert(noexcept(std::declval<SocketResult<int>&>().Value()));
static_assert(noexcept(std::declval<const SocketResult<int>&>().Value()));
static_assert(noexcept(std::declval<SocketResult<int>&&>().Value()));
static_assert(noexcept(std::declval<const SocketResult<int>&&>().Value()));
static_assert(noexcept(std::declval<const SocketResult<int>&>().Error()));
static_assert(noexcept(SocketResult<void>::Success()));
static_assert(noexcept(SocketResult<void>::Failure(SocketError{ 1 })));
static_assert(noexcept(std::declval<const SocketResult<void>&>().Value()));
static_assert(noexcept(std::declval<const SocketResult<void>&>().Error()));

TEST(SocketResult, ZeroBytesIsASuccessfulValue)
{
    auto result = SocketResult<std::size_t>::Success(0);
    ASSERT_TRUE(result);
    EXPECT_TRUE(result.HasValue());
    EXPECT_EQ(result.Value(), 0u);
    EXPECT_TRUE(result.Error().IsSuccess());
    result.Value() = 12;
    const auto copy = result;
    EXPECT_EQ(copy.Value(), 12u);
}

TEST(SocketResult, FailurePreservesErrorWithoutThrowing)
{
    const SocketError error{ std::make_error_code(std::errc::connection_refused) };
    auto result = SocketResult<int>::Failure(error);
    EXPECT_FALSE(result);
    EXPECT_FALSE(result.HasValue());
    EXPECT_EQ(result.Error(), error);
    const auto copy = result;
    EXPECT_FALSE(copy);
    EXPECT_EQ(copy.Error(), error);
}

TEST(SocketResult, MoveOnlyValueCanBeExtractedWithoutCopying)
{
    auto result = SocketResult<std::unique_ptr<int>>::Success(std::make_unique<int>(42));
    auto moved = std::move(result);
    auto value = std::move(moved).Value();
    ASSERT_NE(value, nullptr);
    EXPECT_EQ(*value, 42);
    EXPECT_EQ(moved.Value(), nullptr);
    // Moving the payload does not change the success discriminator.
    EXPECT_TRUE(moved);
}

TEST(SocketResult, AssignmentChangesBetweenSuccessAndFailure)
{
    const SocketError error{ std::make_error_code(std::errc::timed_out) };
    auto result = SocketResult<int>::Success(7);
    result = SocketResult<int>::Failure(error);
    ASSERT_FALSE(result);
    EXPECT_EQ(result.Error(), error);
    result = SocketResult<int>::Success(9);
    ASSERT_TRUE(result);
    EXPECT_EQ(result.Value(), 9);
    EXPECT_TRUE(result.Error().IsSuccess());
}

TEST(SocketResult, VoidSupportsSuccessAndFailure)
{
    auto result = SocketResult<void>::Success();
    EXPECT_TRUE(result);
    EXPECT_TRUE(result.HasValue());
    EXPECT_TRUE(result.Error().IsSuccess());
    result.Value();
    const SocketError error{ std::make_error_code(std::errc::address_in_use) };
    result = SocketResult<void>::Failure(error);
    EXPECT_FALSE(result);
    EXPECT_FALSE(result.HasValue());
    EXPECT_EQ(result.Error(), error);
}

#if GTEST_HAS_DEATH_TEST && !defined(NDEBUG)
TEST(SocketResult, InvalidUsageAsserts)
{
    EXPECT_DEATH((void)SocketResult<int>::Failure(SocketError::Success()), "nonzero error");
    EXPECT_DEATH((void)SocketResult<void>::Failure(SocketError::Success()), "nonzero error");
    const SocketError error{ std::make_error_code(std::errc::timed_out) };
    const auto result = SocketResult<int>::Failure(error);
    EXPECT_DEATH((void)result.Value(), "requires success");
    const auto empty = SocketResult<void>::Failure(error);
    EXPECT_DEATH(empty.Value(), "requires success");
}
#endif

TEST(SocketResult, ConstAndRvalueAccessPreserveValues)
{
    const auto result = SocketResult<int>::Success(42);
    EXPECT_EQ(result.Value(), 42);
    EXPECT_EQ(std::move(result).Value(), 42);
    EXPECT_EQ(SocketResult<int>::Success(7).Value(), 7);
}

TEST(SocketResult, ErrorCanAlsoBeUsedAsTheValueType)
{
    const auto result = SocketResult<SocketError>::Success(SocketError::Success());
    EXPECT_TRUE(result);
    EXPECT_TRUE(result.Value().IsSuccess());
}
