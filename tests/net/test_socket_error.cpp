#include "net/socket_error.hpp"
#include <gtest/gtest.h>
#include <cerrno>
#include <sstream>
#include <system_error>
#include <utility>

TEST(SocketError, DefaultAndFactorySuccessHaveNoError)
{
    const SocketError empty;
    const auto success = SocketError::Success();
    EXPECT_EQ(empty.Code(), 0);
    EXPECT_FALSE(empty.HasError());
    EXPECT_TRUE(empty.IsSuccess());
    EXPECT_FALSE(static_cast<bool>(empty));
    EXPECT_EQ(empty, success);
    EXPECT_EQ(empty, 0);
    EXPECT_EQ(empty, std::errc{});
    EXPECT_FALSE(empty.IsWouldBlock());
    EXPECT_FALSE(empty.IsInterrupted());
    EXPECT_FALSE(empty.IsConnectionReset());
    EXPECT_FALSE(empty.IsConnectionRefused());
    EXPECT_FALSE(empty.IsTimedOut());
    EXPECT_FALSE(empty.IsAddressInUse());
    EXPECT_FALSE(empty.IsConnectionAborted());
}

TEST(SocketError, PreservesCodeCategoryAndValueSemantics)
{
    const auto native = std::error_code(42, std::system_category());
    const SocketError raw{ 42 };
    const SocketError wrapped{ native };
    EXPECT_TRUE(raw.HasError());
    EXPECT_FALSE(raw.IsSuccess());
    EXPECT_TRUE(static_cast<bool>(raw));
    EXPECT_EQ(raw, wrapped);
    EXPECT_EQ(raw, native);
    EXPECT_EQ(raw, 42);
    EXPECT_EQ(raw.ErrorCode(), native);
    EXPECT_FALSE(raw != wrapped);
    EXPECT_FALSE(raw != native);
    EXPECT_FALSE(raw != 42);
    EXPECT_NE(raw, SocketError{ 43 });
    EXPECT_NE(raw, 43);
    EXPECT_NE(raw, std::error_code(43, std::system_category()));
    EXPECT_NE(raw, (SocketError{ std::error_code(42, std::generic_category()) }));
    EXPECT_NE(raw, std::error_code(42, std::generic_category()));
    auto copy = raw;
    EXPECT_EQ(copy, raw);
    copy = SocketError::Success();
    EXPECT_TRUE(copy.IsSuccess());
    EXPECT_TRUE(raw.HasError());
}

TEST(SocketError, MessageAndStreamPreserveStandardLibraryFormatting)
{
    const auto code = std::make_error_code(std::errc::permission_denied);
    const SocketError error{ code };
    EXPECT_EQ(error.Message(), code.message());
    std::ostringstream stream;
    auto& returned = (stream << error);
    EXPECT_EQ(&returned, &stream);
    EXPECT_EQ(stream.str(), error.Message() + " (" + std::to_string(code.value()) + ")");
}

TEST(SocketError, EveryPredicateRejectsUnrelatedErrors)
{
    const SocketError error{ std::make_error_code(std::errc::invalid_argument) };
    EXPECT_FALSE(error.IsWouldBlock());
    EXPECT_FALSE(error.IsInterrupted());
    EXPECT_FALSE(error.IsConnectionReset());
    EXPECT_FALSE(error.IsConnectionRefused());
    EXPECT_FALSE(error.IsTimedOut());
    EXPECT_FALSE(error.IsAddressInUse());
    EXPECT_FALSE(error.IsConnectionAborted());
    EXPECT_FALSE(error.IsTransient());
    EXPECT_FALSE(error.IsDisconnected());
}

TEST(SocketError, CapturesLastPlatformErrorByValue)
{
#ifdef _WIN32
    const int saved = ::WSAGetLastError();
    ::WSASetLastError(WSAECONNREFUSED);
    const auto captured = SocketError::Last();
    ::WSASetLastError(0);
    EXPECT_EQ(captured.Code(), WSAECONNREFUSED);
    EXPECT_TRUE(captured.IsConnectionRefused());
    ::WSASetLastError(saved);
#else
    const int saved = errno;
    errno = ECONNREFUSED;
    const auto captured = SocketError::Last();
    errno = 0;
    EXPECT_EQ(captured.Code(), ECONNREFUSED);
    EXPECT_TRUE(captured.IsConnectionRefused());
    errno = saved;
#endif
}

#ifndef _WIN32
TEST(SocketError, ClassifiesNativeErrnoValues)
{
    EXPECT_TRUE(SocketError{ EWOULDBLOCK }.IsWouldBlock());
    EXPECT_TRUE(SocketError{ EAGAIN }.IsWouldBlock());
    EXPECT_TRUE(SocketError{ EINTR }.IsInterrupted());
    EXPECT_TRUE(SocketError{ ECONNRESET }.IsConnectionReset());
    EXPECT_TRUE(SocketError{ ECONNREFUSED }.IsConnectionRefused());
    EXPECT_TRUE(SocketError{ ETIMEDOUT }.IsTimedOut());
    EXPECT_TRUE(SocketError{ EADDRINUSE }.IsAddressInUse());
    EXPECT_TRUE(SocketError{ ECONNABORTED }.IsConnectionAborted());
    EXPECT_TRUE(SocketError{ EPIPE }.IsDisconnected());
    EXPECT_TRUE(SocketError{ ENOTCONN }.IsDisconnected());
}
#endif

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
        { WSAEMSGSIZE, std::errc::message_size },
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
