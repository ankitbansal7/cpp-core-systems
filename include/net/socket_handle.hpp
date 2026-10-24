#pragma once

#include "net/platform.hpp"
#include "net/socket_error.hpp"

#include <utility>
#include <cstdio>
#include <cstdint>
#include <functional>
#include <compare>

namespace net
{
    class SocketHandle final
    {
    public:
#ifdef _WIN32
        using NativeHandle = SOCKET;
        static constexpr NativeHandle InvalidHandle = INVALID_SOCKET;
#else
        using NativeHandle = int;
        static constexpr NativeHandle InvalidHandle = -1;
#endif

    public:
        SocketHandle() noexcept = default;

        explicit SocketHandle(NativeHandle handle) noexcept
            : m_handle(handle)
        {
        }

        ~SocketHandle() noexcept
        {
            Close();
        }

        // Move-only semantics
        SocketHandle(const SocketHandle&) = delete;
        SocketHandle& operator=(const SocketHandle&) = delete;

        SocketHandle(SocketHandle&& other) noexcept
            : m_handle(other.Release())
        {
        }

        SocketHandle& operator=(SocketHandle&& other) noexcept
        {
            if (this != &other)
            {
                Reset(other.Release());
            }

            return *this;
        }

        [[nodiscard]]
        static SocketHandle Invalid() noexcept
        {
            return SocketHandle{};
        }

        [[nodiscard]]
        bool IsValid() const noexcept
        {
            return m_handle != InvalidHandle;
        }

        [[nodiscard]]
        explicit operator bool() const noexcept
        {
            return IsValid();
        }

        [[nodiscard]]
        NativeHandle Get() const noexcept
        {
            return m_handle;
        }

        [[nodiscard]]
        NativeHandle Release() noexcept
        {
            return std::exchange(m_handle, InvalidHandle);
        }

        void Reset(NativeHandle handle = InvalidHandle) noexcept
        {
            if (m_handle != handle)
            {
                Close();
                m_handle = handle;
            }
        }

        void Close() noexcept
        {
            if (!IsValid())
            {
                return;
            }

            NativeHandle handleToClose = std::exchange(m_handle, InvalidHandle);

#ifdef _WIN32
            int result = ::closesocket(handleToClose);
#else
            int result = ::close(handleToClose);
#endif

#ifndef NDEBUG
            if (result != 0)
            {
                const SocketError error = SocketError::Last();

                try
                {
                    std::fprintf(stderr,
                        "SocketHandle::Close failed (handle=%llu): %s (code=%d)\n",
                        static_cast<unsigned long long>(handleToClose),
                        error.Message().c_str(),
                        error.Code());
                }
                catch (...)
                {
                    std::fprintf(stderr,
                        "SocketHandle::Close failed (handle=%llu, code=%d)\n",
                        static_cast<unsigned long long>(handleToClose),
                        error.Code());
                }
            }
#endif
        }

        void Swap(SocketHandle& other) noexcept
        {
            using std::swap;
            swap(m_handle, other.m_handle);
        }

        friend void swap(SocketHandle& a, SocketHandle& b) noexcept
        {
            a.Swap(b);
        }

        // --- Relational Operators ---

        [[nodiscard]]
        friend bool operator==(const SocketHandle& lhs, const SocketHandle& rhs) noexcept
        {
            return lhs.m_handle == rhs.m_handle;
        }

        [[nodiscard]]
        friend auto operator<=>(const SocketHandle& lhs, const SocketHandle& rhs) noexcept
        {
            return lhs.m_handle <=> rhs.m_handle;
        }

    private:
        NativeHandle m_handle{ InvalidHandle };
    };
}

namespace std
{
    template <>
    struct hash<net::SocketHandle>
    {
        std::size_t operator()(const net::SocketHandle& handle) const noexcept
        {
            return std::hash<net::SocketHandle::NativeHandle>{}(handle.Get());
        }
    };
}
