# cpp-core-systems
Low-level C++ runtime and systems foundations with performance-focused experiments.
This repository is used as a personal reference and learning codebase.

## TCP networking

`TcpSocket` is a move-only blocking TCP connection. Call `Connect(endpoint)` on
a default-constructed socket to create a client. `TcpListener` provides the
server side: call `Bind(endpoint)`, `Listen()`, then `Accept()` for each client.
Each accepted `TcpSocket` owns its connection independently of the listener.
Both support IPv4 and IPv6 numeric endpoints. Keep a `SocketRuntime` alive until
all sockets and listeners have been destroyed.

Operations return `SocketResult<T>` rather than throwing socket errors. Check
the result before calling `Value()`; use `Error()` on failure. `Send` and
`Receive` return the number of bytes transferred and may transfer only part of
the buffer. A successful receive of zero bytes into a nonempty buffer signals
peer EOF. Empty buffers are successful no-ops. `Shutdown(SocketShutdown::Send)`
signals EOF to the peer while allowing responses to be received. `Close()` and
destruction release the native handle. `IsValid()` reports handle ownership,
not whether the peer is still connected.

These initial APIs block and do not impose deadlines or resolve hostnames.
`GetNativeHandle()` borrows the handle for platform-specific socket options;
do not close it directly. Concurrent mutation or closing of the same object
requires external coordination.

[example_tcp_loopback.cpp](examples/net/example_tcp_loopback.cpp) runs a small
client/server echo exchange entirely on loopback using an OS-selected port.
It handles partial transfers and half-close without requiring a separate server.
On Windows, from a Visual Studio developer shell:

```powershell
cmake --preset x64-debug-tests
cmake --build --preset x64-debug-tests --target example_tcp_loopback test_net
.\out\build\x64-debug-tests\examples\example_tcp_loopback.exe
ctest --preset x64-debug-tests -L net
```

In Visual Studio, select **x64 Debug (Tests)**, configure CMake, and select
`example_tcp_loopback.exe` to run the example. The tests share the `test_net`
executable and are discovered individually after building it.

## TCP benchmark

`bench_tcp` measures mean round-trip time for 64-byte messages and one-way
stream throughput with 64 KiB send buffers on IPv4 loopback. It uses a dedicated
server thread, handles partial transfers, and checks errors and received byte
counts. Socket I/O has five-second timeouts. The round-trip measurement excludes
256 warmup exchanges and enables `TCP_NODELAY` on both endpoints. The stream
measurement leaves Nagle's algorithm enabled and ends when the receiver has
consumed all bytes. Connection setup, thread creation, and buffer allocation
are outside the timed sections.

Build the benchmark in Release. From a Visual Studio developer shell:

```powershell
cmake --preset x64-release -DCPP_CORE_SYSTEMS_BUILD_BENCHMARKS=ON
cmake --build out/build/x64-release --target bench_tcp
.\out\build\x64-release\benchmarks\bench_tcp.exe
# Optional: round-trip count and stream size in MiB
.\out\build\x64-release\benchmarks\bench_tcp.exe 1000 16
```

On Linux:

```bash
bash scripts/run-benchmarks.sh
# Optional: round-trip count and stream size in MiB
bash scripts/run-benchmarks.sh 1000 16
```

The script configures a Release build in `out/build/linux-benchmarks`, builds
`bench_tcp`, and forwards arguments to it. Subsequent runs reuse the build
directory. It works from any directory when invoked by its absolute path and
returns a nonzero status if configuration, compilation, or the benchmark fails.
Use `bash scripts/run-benchmarks.sh --help` for workload limits and defaults.

Results include the OS networking stack and thread scheduling costs. They are
end-to-end loopback measurements, not isolated wrapper overhead or physical
network performance. Repeat runs under comparable system load to compare changes.

## Linux setup

Install a C++20-capable compiler, CMake 3.20 or newer, Git, and Make
(or Ninja if selected as your CMake generator). On Ubuntu/Debian:

```bash
sudo apt update
sudo apt install build-essential cmake git
```

## Building on Linux

Build the library and examples with:

```bash
bash scripts/build.sh
```

The default is a Debug build in `out/build/linux-debug`. To build Release:

```bash
bash scripts/build.sh Release
```

Build configurations use separate directories, such as `out/build/linux-release`.
The script also supports `RelWithDebInfo` and `MinSizeRel`. Subsequent runs
rebuild changed files. You can invoke it by its absolute path from any directory.

## Running tests on Linux

From the repository root, run:

```bash
bash scripts/run-tests.sh
```

The script configures a Debug build in `out/build/linux-tests`, builds the
project, and runs all registered container and networking tests. Subsequent
runs reuse the build directory and rebuild changed files. The first run
downloads GoogleTest automatically. Any configuration, build, or test failure
returns a nonzero exit code.

You can also invoke the script by its absolute path from any directory.
Additional arguments go directly to CTest, for example:

```bash
bash scripts/run-tests.sh -L net
bash scripts/run-tests.sh -R SocketHandle
```

## Running clang-tidy on Linux

Install clang-tidy alongside the Linux build prerequisites:

```bash
sudo apt install clang-tidy
bash scripts/run-clang-tidy.sh
```

The script generates a compile database in `out/build/linux-clang-tidy` and
checks the library and example sources, including project headers they use.
The minimal `.clang-tidy` configuration enables Clang static analyzer and
bug-prone code checks. It reports diagnostics without modifying files.
Headers that are not included by these sources are not analyzed.

Additional arguments go directly to clang-tidy. To make enabled warnings fail
the run:

```bash
bash scripts/run-clang-tidy.sh '--warnings-as-errors=*'
```
