#include "net/socket_result.hpp"
#include "net/socket_handle.hpp"

#include <gtest/gtest.h>
#include <memory>
#include <type_traits>
#include <utility>

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

TEST(SocketResult, FailurePreservesErrorAndCheckedAccessThrows)
{
    const SocketError error{ std::make_error_code(std::errc::connection_refused) };
    auto result = SocketResult<int>::Failure(error);
    EXPECT_FALSE(result);
    EXPECT_FALSE(result.HasValue());
    EXPECT_EQ(result.Error(), error);
    try
    {
        (void)result.Value();
        FAIL() << "Expected std::system_error";
    }
    catch (const std::system_error& exception)
    {
        EXPECT_EQ(exception.code(), error.ErrorCode());
    }
    const auto copy = result;
    EXPECT_THROW((void)copy.Value(), std::system_error);
    EXPECT_THROW((void)std::move(result).Value(), std::system_error);
    EXPECT_THROW((void)std::move(copy).Value(), std::system_error);
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
    EXPECT_NO_THROW(result.Value());
    const SocketError error{ std::make_error_code(std::errc::address_in_use) };
    result = SocketResult<void>::Failure(error);
    EXPECT_FALSE(result);
    EXPECT_FALSE(result.HasValue());
    EXPECT_EQ(result.Error(), error);
    EXPECT_THROW(result.Value(), std::system_error);
}

TEST(SocketResult, RejectsFailureWithoutAnError)
{
    EXPECT_THROW((void)SocketResult<int>::Failure(SocketError::Success()), std::invalid_argument);
    EXPECT_THROW((void)SocketResult<void>::Failure(SocketError::Success()), std::invalid_argument);
}

TEST(SocketResult, ErrorCanAlsoBeUsedAsTheValueType)
{
    const auto result = SocketResult<SocketError>::Success(SocketError::Success());
    EXPECT_TRUE(result);
    EXPECT_TRUE(result.Value().IsSuccess());
}
