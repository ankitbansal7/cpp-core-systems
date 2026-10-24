// Standalone TCP loopback benchmark; no external benchmark framework required.
#include "net/socket_runtime.hpp"
#include "net/tcp_listener.hpp"

#include <array>
#include <charconv>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <latch>
#include <span>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

using namespace net;

namespace
{
    using Clock = std::chrono::steady_clock;
    constexpr std::size_t MessageBytes = 64;
    constexpr std::size_t ChunkBytes = 64 * 1024;
    constexpr std::uint64_t MiB = 1024 * 1024;
    constexpr std::uint64_t WarmupRounds = 256;

    struct ConnectionPair
    {
        TcpSocket client;
        TcpSocket server;
    };

    SocketError Configure(TcpSocket& socket, bool noDelay) noexcept
    {
        const auto handle = socket.GetNativeHandle();
#ifdef _WIN32
        const DWORD timeout = 5000;
        const auto* timeoutValue = reinterpret_cast<const char*>(&timeout);
#else
        const timeval timeout{ 5, 0 };
        const auto* timeoutValue = &timeout;
#endif
        if (::setsockopt(handle, SOL_SOCKET, SO_RCVTIMEO, timeoutValue, sizeof(timeout)) != 0 ||
            ::setsockopt(handle, SOL_SOCKET, SO_SNDTIMEO, timeoutValue, sizeof(timeout)) != 0)
        {
            return SocketError::Last();
        }
        const int enabled = noDelay ? 1 : 0;
#ifdef _WIN32
        const auto* optionValue = reinterpret_cast<const char*>(&enabled);
#else
        const auto* optionValue = &enabled;
#endif
        if (::setsockopt(handle, IPPROTO_TCP, TCP_NODELAY, optionValue, sizeof(enabled)) != 0)
        {
            return SocketError::Last();
        }
        return SocketError::Success();
    }

    SocketResult<ConnectionPair> ConnectPair(bool noDelay) noexcept
    {
        TcpListener listener;
        const auto bound = listener.Bind({ IPAddress::Loopback(IPVersion::IPv4), 0 });
        if (!bound)
        {
            return SocketResult<ConnectionPair>::Failure(bound.Error());
        }
        const auto listening = listener.Listen();
        if (!listening)
        {
            return SocketResult<ConnectionPair>::Failure(listening.Error());
        }
        const auto endpoint = listener.GetLocalEndpoint();
        if (!endpoint)
        {
            return SocketResult<ConnectionPair>::Failure(endpoint.Error());
        }
        ConnectionPair pair;
        const auto connected = pair.client.Connect(endpoint.Value());
        if (!connected)
        {
            return SocketResult<ConnectionPair>::Failure(connected.Error());
        }
        auto accepted = listener.Accept();
        if (!accepted)
        {
            return SocketResult<ConnectionPair>::Failure(accepted.Error());
        }
        pair.server = std::move(accepted).Value();
        for (auto* socket : { &pair.client, &pair.server })
        {
            const auto error = Configure(*socket, noDelay);
            if (error)
            {
                return SocketResult<ConnectionPair>::Failure(error);
            }
        }
        return SocketResult<ConnectionPair>::Success(std::move(pair));
    }

    SocketError SendAll(TcpSocket& socket, std::span<const std::byte> bytes) noexcept
    {
        while (!bytes.empty())
        {
            const auto result = socket.Send(bytes);
            if (!result)
            {
                return result.Error();
            }
            if (result.Value() == 0)
            {
                return SocketError{ std::make_error_code(std::errc::io_error) };
            }
            bytes = bytes.subspan(result.Value());
        }
        return SocketError::Success();
    }

    SocketError ReceiveExact(TcpSocket& socket, std::span<std::byte> bytes) noexcept
    {
        while (!bytes.empty())
        {
            const auto result = socket.Receive(bytes);
            if (!result)
            {
                return result.Error();
            }
            if (result.Value() == 0)
            {
                return SocketError{ std::make_error_code(std::errc::io_error) };
            }
            bytes = bytes.subspan(result.Value());
        }
        return SocketError::Success();
    }

    bool ReportErrors(const SocketError& client, const SocketError& server)
    {
        if (client) { std::cerr << "Client failed: " << client << '\n'; }
        if (server) { std::cerr << "Server failed: " << server << '\n'; }
        return !client && !server;
    }

    bool RoundTrips(std::uint64_t rounds)
    {
        auto connected = ConnectPair(true);
        if (!connected)
        {
            std::cerr << "Round-trip setup failed: " << connected.Error() << '\n';
            return false;
        }
        auto pair = std::move(connected).Value();
        std::array<std::byte, MessageBytes> request{};
        request.fill(std::byte{0x5a});
        std::array<std::byte, MessageBytes> reply{};
        std::array<std::byte, MessageBytes> echo{};
        SocketError serverError;
        std::jthread worker([&]
        {
            for (std::uint64_t index = 0; index < WarmupRounds + rounds; ++index)
            {
                serverError = ReceiveExact(pair.server, echo);
                if (!serverError) { serverError = SendAll(pair.server, echo); }
                if (serverError)
                {
                    (void)pair.server.Shutdown(); // Unblock the client on failure.
                    break;
                }
            }
        });
        const auto exchange = [&](std::uint64_t count)
        {
            for (std::uint64_t index = 0; index < count; ++index)
            {
                auto error = SendAll(pair.client, request);
                if (!error) { error = ReceiveExact(pair.client, reply); }
                if (error) { return error; }
            }
            return SocketError::Success();
        };
        auto clientError = exchange(WarmupRounds);
        const auto start = Clock::now();
        if (!clientError) { clientError = exchange(rounds); }
        const auto end = Clock::now();
        if (clientError) { (void)pair.client.Shutdown(); }
        worker.join();
        if (!ReportErrors(clientError, serverError)) { return false; }
        if (reply != request)
        {
            std::cerr << "Echo payload mismatch\n";
            return false;
        }
        const double seconds = std::chrono::duration<double>(end - start).count();
        std::cout << "Round trip: " << rounds << " exchanges, " << MessageBytes
            << " bytes each way, TCP_NODELAY=on\n"
            << "  Mean RTT: " << seconds * 1e6 / static_cast<double>(rounds)
            << " us; exchanges/s: " << static_cast<double>(rounds) / seconds << '\n';
        return true;
    }

    bool Throughput(std::uint64_t streamMiB)
    {
        auto connected = ConnectPair(false);
        if (!connected)
        {
            std::cerr << "Throughput setup failed: " << connected.Error() << '\n';
            return false;
        }
        auto pair = std::move(connected).Value();
        const auto total = streamMiB * MiB;
        std::vector<std::byte> sendBuffer(ChunkBytes, std::byte{0x5a});
        std::vector<std::byte> receiveBuffer(ChunkBytes);
        SocketError serverError;
        std::uint64_t received = 0;
        std::latch ready{1};
        std::latch finished{1};
        std::jthread worker([&]
        {
            ready.count_down();
            while (received < total)
            {
                const auto result = pair.server.Receive(receiveBuffer);
                if (!result)
                {
                    serverError = result.Error();
                    break;
                }
                if (result.Value() == 0)
                {
                    serverError = SocketError{ std::make_error_code(std::errc::io_error) };
                    break;
                }
                received += result.Value();
            }
            if (serverError) { (void)pair.server.Shutdown(); }
            finished.count_down();
        });
        ready.wait();
        SocketError clientError;
        const auto start = Clock::now();
        for (std::uint64_t sent = 0; sent < total; sent += ChunkBytes)
        {
            clientError = SendAll(pair.client, sendBuffer);
            if (clientError) { break; }
        }
        if (clientError) { (void)pair.client.Shutdown(); }
        finished.wait(); // Measure delivery to the receiver, not just queued sends.
        const auto end = Clock::now();
        worker.join();
        if (!ReportErrors(clientError, serverError)) { return false; }
        if (received != total)
        {
            std::cerr << "Stream byte count mismatch\n";
            return false;
        }
        const double seconds = std::chrono::duration<double>(end - start).count();
        std::cout << "Stream: " << streamMiB << " MiB, " << ChunkBytes
            << "-byte send chunks, TCP_NODELAY=off\n"
            << "  Throughput: " << static_cast<double>(streamMiB) / seconds
            << " MiB/s; elapsed: " << seconds << " s\n";
        return true;
    }

    bool ParseCount(std::string_view text, std::uint64_t maximum, std::uint64_t& output)
    {
        const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), output);
        return error == std::errc{} && end == text.data() + text.size() && output > 0 && output <= maximum;
    }

    void Usage()
    {
        std::cout << "Usage: bench_tcp [round-trips: 1..1000000] [stream-MiB: 1..4096]\n"
            << "Defaults: 10000 round trips, 128 MiB stream. IPv4 loopback only.\n";
    }
}

int main(int argc, char** argv)
{
    if (argc == 2 && std::string_view{argv[1]} == "--help")
    {
        Usage();
        return 0;
    }
    std::uint64_t rounds = 10000;
    std::uint64_t streamMiB = 128;
    if (argc > 3 || (argc > 1 && !ParseCount(argv[1], 1000000, rounds)) ||
        (argc > 2 && !ParseCount(argv[2], 4096, streamMiB)))
    {
        Usage();
        return 1;
    }
    SocketRuntime runtime;
    if (!runtime)
    {
        std::cerr << "Runtime initialization failed: " << runtime.GetError() << '\n';
        return 1;
    }
#ifndef NDEBUG
    std::cout << "Debug build: use Release for performance measurements.\n";
#endif
    std::cout << std::fixed << std::setprecision(3);
    return RoundTrips(rounds) && Throughput(streamMiB) ? 0 : 1;
}
