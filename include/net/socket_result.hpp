#pragma once

#include "net/socket_error.hpp"

#include <cassert>
#include <type_traits>
#include <utility>
#include <variant>

namespace net
{
    // A socket operation returns either a value or a nonzero error.
    // Use SocketResult<void> for operations such as Connect and Bind.
    // bool means success, unlike SocketError's bool (which means error).
    template <typename T>
    class [[nodiscard]] SocketResult final
    {
        static_assert(std::is_object_v<T> && !std::is_array_v<T> &&
            !std::is_const_v<T> && !std::is_volatile_v<T>,
            "SocketResult<T> requires an unqualified, non-array object type");

    public:
        [[nodiscard]]
        static SocketResult Success(T value) noexcept(std::is_nothrow_move_constructible_v<T>)
        {
            return SocketResult{ std::in_place_index<0>, std::move(value) };
        }

        [[nodiscard]]
        static SocketResult Failure(SocketError error) noexcept
        {
            assert(error.HasError() && "SocketResult::Failure requires a nonzero error");
            return SocketResult{ std::in_place_index<1>, error };
        }

        [[nodiscard]]
        bool HasValue() const noexcept
        {
            return m_storage.index() == 0;
        }

        [[nodiscard]]
        explicit operator bool() const noexcept
        {
            return HasValue();
        }

        // Precondition: HasValue(). Check the result before accessing its value.
        // Invalid access asserts in debug builds and is undefined in release builds.
        [[nodiscard]]
        T& Value() & noexcept
        {
            CheckValue();
            return *std::get_if<0>(&m_storage);
        }

        [[nodiscard]]
        const T& Value() const & noexcept
        {
            CheckValue();
            return *std::get_if<0>(&m_storage);
        }

        [[nodiscard]]
        T&& Value() && noexcept
        {
            CheckValue();
            return std::move(*std::get_if<0>(&m_storage));
        }

        [[nodiscard]]
        const T&& Value() const && noexcept
        {
            CheckValue();
            return std::move(*std::get_if<0>(&m_storage));
        }

        // Returns a success error code when a value is present.
        [[nodiscard]]
        SocketError Error() const noexcept
        {
            if (HasValue())
            {
                return SocketError::Success();
            }
            const auto* error = std::get_if<1>(&m_storage);
            assert(error != nullptr && "SocketResult storage is valueless");
            return *error;
        }

    private:
        template <std::size_t Index, typename U>
        SocketResult(std::in_place_index_t<Index> tag, U&& value)
            noexcept(std::is_nothrow_constructible_v<std::variant<T, SocketError>,
                std::in_place_index_t<Index>, U&&>)
            : m_storage(tag, std::forward<U>(value))
        {
        }

        void CheckValue() const noexcept
        {
            assert(HasValue() && "SocketResult::Value requires success");
        }

        // Construction/copy/move inherit T's exception behavior. A throwing T during
        // assignment can leave storage valueless; Value/Error must not then be used.
        std::variant<T, SocketError> m_storage;
    };

    template <>
    class [[nodiscard]] SocketResult<void> final
    {
    public:
        [[nodiscard]]
        static SocketResult Success() noexcept
        {
            return SocketResult{ SocketError::Success() };
        }

        [[nodiscard]]
        static SocketResult Failure(SocketError error) noexcept
        {
            assert(error.HasError() && "SocketResult::Failure requires a nonzero error");
            return SocketResult{ error };
        }

        [[nodiscard]]
        bool HasValue() const noexcept
        {
            return m_error.IsSuccess();
        }

        [[nodiscard]]
        explicit operator bool() const noexcept
        {
            return HasValue();
        }

        // Precondition: HasValue(), as for the value-bearing specialization.
        void Value() const noexcept
        {
            assert(HasValue() && "SocketResult::Value requires success");
        }

        [[nodiscard]]
        SocketError Error() const noexcept
        {
            return m_error;
        }

    private:
        explicit SocketResult(SocketError error) noexcept
            : m_error(error)
        {
        }

        SocketError m_error;
    };
}
