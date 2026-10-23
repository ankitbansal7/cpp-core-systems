#pragma once

#include "net/socket_handle.hpp"
#include <gtest/gtest.h>

#ifndef _WIN32
#include <sys/select.h>
#endif

namespace socket_test
{
    inline bool SetTimeouts(SocketHandle::NativeHandle handle)
    {
#ifdef _WIN32
        const DWORD timeout = 2000;
        const auto* value = reinterpret_cast<const char*>(&timeout);
#else
        const timeval timeout{ 2, 0 };
        const auto* value = &timeout;
#endif
        return ::setsockopt(handle, SOL_SOCKET, SO_RCVTIMEO, value, sizeof(timeout)) == 0 &&
            ::setsockopt(handle, SOL_SOCKET, SO_SNDTIMEO, value, sizeof(timeout)) == 0;
    }

    inline bool IsReadable(SocketHandle::NativeHandle handle)
    {
        fd_set readSet;
        FD_ZERO(&readSet);
        FD_SET(handle, &readSet);
        timeval timeout{ 2, 0 };
#ifdef _WIN32
        const int count = ::select(0, &readSet, nullptr, nullptr, &timeout);
#else
        const int count = ::select(handle + 1, &readSet, nullptr, nullptr, &timeout);
#endif
        return count == 1;
    }
}
