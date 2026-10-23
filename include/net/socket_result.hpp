#pragma once

#include "net/socket_error.hpp"

#include <stdexcept>
#include <type_traits>
#include <utility>
#include <variant>

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
    static SocketResult Success(T value)
    {
        return SocketResult{ std::in_place_index<0>, std::move(value) };
    }

    [[nodiscard]]
    static SocketResult Failure(SocketError error)
    {
        if (!error)
        {
            throw std::invalid_argument("SocketResult::Failure requires a nonzero error");
        }
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

    // Checked access: throws std::system_error on failure.
    [[nodiscard]]
    T& Value() &
    {
        CheckValue();
        return std::get<0>(m_storage);
    }

    [[nodiscard]]
    const T& Value() const &
    {
        CheckValue();
        return std::get<0>(m_storage);
    }

    [[nodiscard]]
    T&& Value() &&
    {
        CheckValue();
        return std::get<0>(std::move(m_storage));
    }

    [[nodiscard]]
    const T&& Value() const &&
    {
        CheckValue();
        return std::get<0>(std::move(m_storage));
    }

    // Returns a success error code when a value is present.
    [[nodiscard]]
    SocketError Error() const
    {
        return HasValue() ? SocketError::Success() : std::get<1>(m_storage);
    }

private:
    template <std::size_t Index, typename U>
    SocketResult(std::in_place_index_t<Index> tag, U&& value)
        : m_storage(tag, std::forward<U>(value))
    {
    }

    void CheckValue() const
    {
        if (!HasValue())
        {
            throw std::system_error(Error().ErrorCode());
        }
    }

    // Copy/move support follows T. As with std::variant, a throwing T during
    // assignment can leave storage valueless; access then throws bad_variant_access.
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
    static SocketResult Failure(SocketError error)
    {
        if (!error)
        {
            throw std::invalid_argument("SocketResult::Failure requires a nonzero error");
        }
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

    void Value() const
    {
        if (m_error)
        {
            throw std::system_error(m_error.ErrorCode());
        }
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
