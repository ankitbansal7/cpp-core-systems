#pragma once

#include "net/platform.hpp"
#include "net/socket_error.hpp"

#include <utility>
#include <cstdio>
#include <cstdint>

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

    ~SocketHandle()
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
            SocketError error = SocketError::Last();
            std::fprintf(stderr,
                "SocketHandle::Close failed (handle=%llu): %s\n",
                static_cast<unsigned long long>(handleToClose),
                error.Message().c_str());
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

private:
    NativeHandle m_handle{ InvalidHandle };
};

